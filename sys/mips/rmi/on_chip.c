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
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/limits.h>
#include <sys/bus.h>

#include <machine/reg.h>
#include <machine/cpu.h>
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

int xlr_counters[MAXCPU][XLR_MAX_COUNTERS] __aligned(XLR_CACHELINE_SIZE);

void xlr_msgring_handler(struct trapframe *);

void 
xlr_msgring_cpu_init(void)
{
	struct stn_cc *cc_config;
	struct bucket_size *bucket_sizes;
	int id;
	unsigned long flags;

	/* if not thread 0 */
	if (xlr_thr_id() != 0)
		return;
	id = xlr_cpu_id();

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
/*   printf("[%s]: int_type = 0x%x, pop_num_buckets=%d, pop_bucket_mask=%x" */
/*          "watermark_count=%d, thread_mask=%x\n", __FUNCTION__, */
/*          msgring_int_type, msgring_pop_num_buckets, msgring_pop_bucket_mask, */
/*          msgring_watermark_count, msgring_thread_mask); */
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

	xlr_inc_counter(MSGRNG_INT);
	/* TODO: not necessary to disable preemption */
	msgrng_flags_save(mflags);

	/* First Drain all the high priority messages */
	for (;;) {
		bucket_empty_bm = (msgrng_read_status() >> 24) & msgring_pop_bucket_mask;

		/* all buckets empty, break */
		if (bucket_empty_bm == msgring_pop_bucket_mask)
			break;

		for (bucket = 0; bucket < msgring_pop_num_buckets; bucket++) {
			uint32_t cycles = 0;

			if ((bucket_empty_bm & (1 << bucket)) /* empty */ )
				continue;

			status = message_receive(bucket, &size, &code, &rx_stid, &msg);
			if (status)
				continue;

			xlr_inc_counter(MSGRNG_MSG);
			msgrng_msg_cycles = mips_rd_count();
			cycles = msgrng_msg_cycles;

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
			xlr_set_counter(MSGRNG_MSG_CYCLES, (read_c0_count() - cycles));
		}
	}

	xlr_set_counter(MSGRNG_EXIT_STATUS, msgrng_read_status());

	msgrng_flags_restore(mflags);

	//dbg_msg("OUT irq=%d\n", irq);

	/* Call the msg callback */
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

extern void platform_prep_smp_launch(void);
extern void msgring_process_fast_intr(void *arg);

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
		platform_prep_smp_launch();

		cpu_establish_hardintr("msgring", (driver_filter_t *) NULL,
		    (driver_intr_t *) msgring_process_fast_intr,
		    NULL, IRQ_MSGRING, INTR_TYPE_NET | INTR_FAST, &cookie);

		/* configure the msgring interrupt on cpu 0 */
		enable_msgring_int(NULL);
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
		xlr_write_reg(mmio, PIC_IRT_1_BASE + i, (level << 30) | (1 << 6) | (PIC_IRQ_BASE + i));
	}
	dbg_msg("PIC init now done\n");
}

void 
on_chip_init(void)
{
	int i = 0, j = 0;

	/* Set xlr_io_base to the run time value */
	mtx_init(&msgrng_lock, "msgring", NULL, MTX_SPIN | MTX_RECURSE);
	mtx_init(&xlr_pic_lock, "pic", NULL, MTX_SPIN);

	xlr_board_info_setup();

	msgring_int_enabled = 0;

	xlr_msgring_config();
	pic_init();

	xlr_msgring_cpu_init();

	for (i = 0; i < MAXCPU; i++)
		for (j = 0; j < XLR_MAX_COUNTERS; j++)
			atomic_set_int(&xlr_counters[i][j], 0);
}
