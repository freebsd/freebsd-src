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
#include <mips/rmi/iomap.h>
#include <mips/rmi/debug.h>
#include <mips/rmi/pic.h>
#include <mips/rmi/board.h>

void 
disable_msgring_int(void *arg);
void 
enable_msgring_int(void *arg);

/* definitions */
struct tx_stn_handler {
	void (*action) (int, int, int, int, struct msgrng_msg *, void *);
	void *dev_id;
};

struct msgring_ithread {
	struct thread *i_thread;
	u_int i_pending;
	u_int i_flags;
	int i_cpu;
};

struct msgring_ithread *msgring_ithreads[MAXCPU];

/* globals */
static struct tx_stn_handler tx_stn_handlers[MAX_TX_STNS];

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


/* make this a read/write spinlock */
static struct mtx msgrng_lock;
static int msgring_int_enabled;
struct mtx xlr_pic_lock;

static int msgring_pop_num_buckets;
static uint32_t msgring_pop_bucket_mask;
static int msgring_int_type;
static int msgring_watermark_count;
static uint32_t msgring_thread_mask;

uint32_t msgrng_msg_cycles = 0;

void xlr_msgring_handler(struct trapframe *);

void 
xlr_msgring_cpu_init(void)
{
	struct stn_cc *cc_config;
	struct bucket_size *bucket_sizes;
	int id;
	unsigned long flags;

	KASSERT(xlr_thr_id() == 0,
		("xlr_msgring_cpu_init from non-zero thread\n"));

	id = xlr_core_id();

	bucket_sizes = xlr_board_info.bucket_sizes;
	cc_config = xlr_board_info.credit_configs[id];

	msgrng_flags_save(flags);

	/*
	 * Message Stations are shared among all threads in a cpu core
	 * Assume, thread 0 on all cores are always active when more than 1
	 * thread is active in a core
	 */
	msgrng_write_bucksize(0, bucket_sizes->bucket[id * 8 + 0]);
	msgrng_write_bucksize(1, bucket_sizes->bucket[id * 8 + 1]);
	msgrng_write_bucksize(2, bucket_sizes->bucket[id * 8 + 2]);
	msgrng_write_bucksize(3, bucket_sizes->bucket[id * 8 + 3]);
	msgrng_write_bucksize(4, bucket_sizes->bucket[id * 8 + 4]);
	msgrng_write_bucksize(5, bucket_sizes->bucket[id * 8 + 5]);
	msgrng_write_bucksize(6, bucket_sizes->bucket[id * 8 + 6]);
	msgrng_write_bucksize(7, bucket_sizes->bucket[id * 8 + 7]);

	MSGRNG_CC_INIT_CPU_DEST(0, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(1, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(2, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(3, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(4, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(5, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(6, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(7, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(8, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(9, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(10, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(11, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(12, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(13, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(14, cc_config->counters);
	MSGRNG_CC_INIT_CPU_DEST(15, cc_config->counters);

	msgrng_flags_restore(flags);
}

void 
xlr_msgring_config(void)
{
	msgring_int_type = 0x02;
	msgring_pop_num_buckets = 8;
	msgring_pop_bucket_mask = 0xff;

	msgring_watermark_count = 1;
	msgring_thread_mask = 0x01;
}

void 
xlr_msgring_handler(struct trapframe *tf)
{
	unsigned long mflags;
	int bucket = 0;
	int size = 0, code = 0, rx_stid = 0, tx_stid = 0;
	struct msgrng_msg msg;
	unsigned int bucket_empty_bm = 0;
	unsigned int status = 0;

	/* TODO: not necessary to disable preemption */
	msgrng_flags_save(mflags);

	/* First Drain all the high priority messages */
	for (;;) {
		bucket_empty_bm = (msgrng_read_status() >> 24) & msgring_pop_bucket_mask;

		/* all buckets empty, break */
		if (bucket_empty_bm == msgring_pop_bucket_mask)
			break;

		for (bucket = 0; bucket < msgring_pop_num_buckets; bucket++) {
			if ((bucket_empty_bm & (1 << bucket)) /* empty */ )
				continue;

			status = message_receive(bucket, &size, &code, &rx_stid, &msg);
			if (status)
				continue;

			tx_stid = xlr_board_info.msgmap[rx_stid];

			if (!tx_stn_handlers[tx_stid].action) {
				printf("[%s]: No Handler for message from stn_id=%d, bucket=%d, "
				    "size=%d, msg0=%llx, dropping message\n",
				    __FUNCTION__, tx_stid, bucket, size, msg.msg0);
			} else {
				//printf("[%s]: rx_stid = %d\n", __FUNCTION__, rx_stid);
				msgrng_flags_restore(mflags);
				(*tx_stn_handlers[tx_stid].action) (bucket, size, code, rx_stid,
				    &msg, tx_stn_handlers[tx_stid].dev_id);
				msgrng_flags_save(mflags);
			}
		}
	}

	xlr_set_counter(MSGRNG_EXIT_STATUS, msgrng_read_status());

	msgrng_flags_restore(mflags);
}

void 
enable_msgring_int(void *arg)
{
	unsigned long mflags = 0;

	msgrng_access_save(&msgrng_lock, mflags);
	/* enable the message ring interrupts */
	msgrng_write_config((msgring_watermark_count << 24) | (IRQ_MSGRING << 16)
	    | (msgring_thread_mask << 8) | msgring_int_type);
	msgrng_access_restore(&msgrng_lock, mflags);
}

void 
disable_msgring_int(void *arg)
{
	unsigned long mflags = 0;
	uint32_t config;

	msgrng_access_save(&msgrng_lock, mflags);
	config = msgrng_read_config();
	config &= ~0x3;
	msgrng_write_config(config);
	msgrng_access_restore(&msgrng_lock, mflags);
}

static int
msgring_process_fast_intr(void *arg)
{
	int cpu = PCPU_GET(cpuid);
	volatile struct msgring_ithread *it;
	struct thread *td;

	/* wakeup an appropriate intr_thread for processing this interrupt */
	it = (volatile struct msgring_ithread *)msgring_ithreads[cpu];
	KASSERT(it != NULL, ("No interrupt thread on cpu %d", cpu));
	td = it->i_thread;

	/*
	 * Interrupt thread will enable the interrupts after processing all
	 * messages
	 */
	disable_msgring_int(NULL);
	atomic_store_rel_int(&it->i_pending, 1);
	thread_lock(td);
	if (TD_AWAITING_INTR(td)) {
		TD_CLR_IWAIT(td);
		sched_add(td, SRQ_INTR);
	}
	thread_unlock(td);
	return FILTER_HANDLED;
}

static void
msgring_process(void *arg)
{
	volatile struct msgring_ithread *ithd;
	struct thread *td;
	struct proc *p;

	td = curthread;
	p = td->td_proc;
	ithd = (volatile struct msgring_ithread *)arg;
	KASSERT(ithd->i_thread == td,
	    ("%s:msg_ithread and proc linkage out of sync", __func__));

	/* First bind this thread to the right CPU */
	thread_lock(td);
	sched_bind(td, ithd->i_cpu);
	thread_unlock(td);

	atomic_store_rel_ptr((volatile uintptr_t *)&msgring_ithreads[ithd->i_cpu],
	     (uintptr_t)arg);
	enable_msgring_int(NULL);
	
	while (1) {
		while (ithd->i_pending) {
			/*
			 * This might need a full read and write barrier to
			 * make sure that this write posts before any of the
			 * memory or device accesses in the handlers.
			 */
			xlr_msgring_handler(NULL);
			atomic_store_rel_int(&ithd->i_pending, 0);
			enable_msgring_int(NULL);
		}
		if (!ithd->i_pending) {
			thread_lock(td);
			if (ithd->i_pending) {
			  thread_unlock(td);
			  continue;
			}
			sched_class(td, PRI_ITHD);
			TD_SET_IWAIT(td);
			mi_switch(SW_VOL, NULL);
			thread_unlock(td);
		}
	}

}

static void 
create_msgring_thread(int cpu)
{
	struct msgring_ithread *ithd;
	struct thread *td;
	struct proc *p;
	int error;

	/* Create kernel thread for message ring interrupt processing */
	/* Currently create one task for thread 0 of each core */
	ithd = malloc(sizeof(struct msgring_ithread),
	    M_DEVBUF, M_WAITOK | M_ZERO);
	error = kproc_create(msgring_process, (void *)ithd, &p,
	    RFSTOPPED | RFHIGHPID, 2, "msg_intr%d", cpu);

	if (error)
		panic("kproc_create() failed with %d", error);
	td = FIRST_THREAD_IN_PROC(p);	/* XXXKSE */

	ithd->i_thread = td;
	ithd->i_pending = 0;
	ithd->i_cpu = cpu;

	thread_lock(td);
	sched_class(td, PRI_ITHD);
	sched_add(td, SRQ_INTR);
	thread_unlock(td);
	CTR2(KTR_INTR, "%s: created %s", __func__, ithd_name[cpu]);
}

int 
register_msgring_handler(int major,
    void (*action) (int, int, int, int, struct msgrng_msg *, void *),
    void *dev_id)
{
	void *cookie;		/* FIXME - use? */

	if (major >= MAX_TX_STNS)
		return 1;

	//dbg_msg("major=%d, action=%p, dev_id=%p\n", major, action, dev_id);

	if (rmi_spin_mutex_safe)
	  mtx_lock_spin(&msgrng_lock);
	tx_stn_handlers[major].action = action;
	tx_stn_handlers[major].dev_id = dev_id;
	if (rmi_spin_mutex_safe)
	  mtx_unlock_spin(&msgrng_lock);

	if (xlr_test_and_set(&msgring_int_enabled)) {
		create_msgring_thread(0);
		cpu_establish_hardintr("msgring", (driver_filter_t *) msgring_process_fast_intr,
			NULL, NULL, IRQ_MSGRING, 
			INTR_TYPE_NET | INTR_FAST, &cookie);
	}
	return 0;
}

static void 
pic_init(void)
{
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);
	int i = 0;
	int level;

	dbg_msg("Initializing PIC...\n");
	for (i = 0; i < PIC_NUM_IRTS; i++) {

		level = PIC_IRQ_IS_EDGE_TRIGGERED(i);

		/* Bind all PIC irqs to cpu 0 */
		xlr_write_reg(mmio, PIC_IRT_0_BASE + i, 0x01);

		/*
		 * Use local scheduling and high polarity for all IRTs
		 * Invalidate all IRTs, by default
		 */
		xlr_write_reg(mmio, PIC_IRT_1_BASE + i, (level << 30) | (1 << 6) |
		    (PIC_IRQ_BASE + i));
	}
	dbg_msg("PIC init now done\n");
}

void 
on_chip_init(void)
{
	/* Set xlr_io_base to the run time value */
	mtx_init(&msgrng_lock, "msgring", NULL, MTX_SPIN | MTX_RECURSE);
	mtx_init(&xlr_pic_lock, "pic", NULL, MTX_SPIN);

	xlr_board_info_setup();

	msgring_int_enabled = 0;

	xlr_msgring_config();
	pic_init();

	xlr_msgring_cpu_init();
}

static void
start_msgring_threads(void *arg)
{
	uint32_t cpu_mask;
	int cpu;

	cpu_mask = PCPU_GET(cpumask) | PCPU_GET(other_cpus);
	for (cpu = 4; cpu < MAXCPU; cpu += 4)
		if (cpu_mask & (1<<cpu))
			create_msgring_thread(cpu);
}

SYSINIT(start_msgring_threads, SI_SUB_SMP, SI_ORDER_MIDDLE, start_msgring_threads, NULL);
