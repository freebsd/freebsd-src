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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/smp.h>
#include <sys/pcpu.h>
#include <mips/rmi/xlrconfig.h>
#include <mips/rmi/perfmon_xlrconfig.h>
#include <mips/rmi/perfmon.h>
#include <mips/rmi/perfmon_utils.h>
#include <mips/rmi/pic.h>
#include <mips/rmi/msgring.h>

#define CC_SAMPLE   (PERF_CP2_CREDITS <<24)

#define CC_REG0 16
#define CC_REG1 17
#define CC_REG2 18
#define CC_REG3 19
#define CC_REG4 20
#define CC_REG5 21
#define CC_REG6 22
#define CC_REG7 23
#define CC_REG8 24
#define CC_REG9 25
#define CC_REG10 26
#define CC_REG11 27
#define CC_REG12 28
#define CC_REG13 29
#define CC_REG14 30
#define CC_REG15 31

extern uint32_t cpu_ltop_map[MAXCPU];
extern struct perf_area *xlr_shared_config_area;

static __inline__ uint32_t 
make_cpu_tag(uint32_t val)
{
	return PERF_CP0_COUNTER << 24 | (val & 0xffff);
}

static __inline__ uint32_t 
make_cp0_perf_control(uint32_t flags, uint32_t thread, uint32_t event)
{
	return (flags & 0x1f) | (thread & 0x03) << 11 | (event & 0x3f) << 5 | 0x01;
}

static __inline__ uint32_t 
cp0_perf_control_get_thread(uint32_t control_word)
{
	return (control_word & 0x1800) >> 11;
}

static __inline__ uint32_t 
cp0_perf_control_get_event(uint32_t control_word)
{
	return (control_word & 0x7e0) >> 5;
}

static __inline__ uint32_t 
read_pic_6_timer_count(void)
{
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);

	/* PIC counts down, convert it to count up */
	return 0xffffffffU - xlr_read_reg(mmio, PIC_TIMER_6_COUNTER_0);
}


static uint32_t 
get_num_events(const uint64_t * events)
{
	int total = 0;
	int thread;

	for (thread = 0; thread < NTHREADS; thread++) {
		if (events[thread] == 0)
			continue;
		total += get_set_bit_count64(events[thread]);
	}
	return total;
}

static uint32_t 
get_first_control_word(uint32_t flags, const uint64_t * events)
{
	int thread, event;

	for (thread = 0; thread < NTHREADS; thread++) {
		if (events[thread] != 0)
			break;
	}
	if (thread == NTHREADS)
		return -1;

	event = find_first_set_bit64(events[thread]);
	return make_cp0_perf_control(flags, thread, event);
}

static uint32_t 
get_next_control_word(uint32_t current_control_word, const uint64_t * events)
{
	int thread = cp0_perf_control_get_thread(current_control_word);
	int event = cp0_perf_control_get_event(current_control_word);
	int i;

	event = find_next_set_bit64(events[thread], event);
	for (i = 0; event == -1 && i < NTHREADS; i++) {
		thread = (thread + 1) % NTHREADS;
		if (events[thread] == 0)
			continue;
		event = find_first_set_bit64(events[thread]);
	}

	ASSERT(event != -1);
	return make_cp0_perf_control(current_control_word, thread, event);
}

/* Global state per core */
#define MY_CORE_NUM (cpu_ltop_map[PCPU_GET(cpuid)]/NTHREADS)
#define my_perf_area (&(xlr_shared_config_area[MY_CORE_NUM]))

static int num_events_array[NCORES];
static uint32_t saved_timestamp_array[NCORES];
static struct perf_config_data saved_config_array[NCORES];
static int cc_sample_array[NCORES];

#define num_events (num_events_array[MY_CORE_NUM])
#define	saved_timestamp (saved_timestamp_array[MY_CORE_NUM])
#define saved_config (saved_config_array[MY_CORE_NUM])
#define cc_sample (cc_sample_array[MY_CORE_NUM])

static void 
do_sample_cc_registers(struct sample_q *q, uint32_t mask)
{
	unsigned long flags;

	DPRINT("Sample CC registers %x", mask);
	msgrng_flags_save(flags);
	if (mask & 0x00000001)
		put_sample(q, CC_SAMPLE + 0, read_cc_registers_0123(CC_REG0), 0);
	if (mask & 0x00000002)
		put_sample(q, CC_SAMPLE + 1, read_cc_registers_4567(CC_REG0), 0);
	if (mask & 0x00000004)
		put_sample(q, CC_SAMPLE + 2, read_cc_registers_0123(CC_REG1), 0);
	if (mask & 0x00000008)
		put_sample(q, CC_SAMPLE + 3, read_cc_registers_4567(CC_REG1), 0);
	if (mask & 0x00000010)
		put_sample(q, CC_SAMPLE + 4, read_cc_registers_0123(CC_REG2), 0);
	if (mask & 0x00000020)
		put_sample(q, CC_SAMPLE + 5, read_cc_registers_4567(CC_REG2), 0);
	if (mask & 0x00000040)
		put_sample(q, CC_SAMPLE + 6, read_cc_registers_0123(CC_REG3), 0);
	if (mask & 0x00000080)
		put_sample(q, CC_SAMPLE + 7, read_cc_registers_4567(CC_REG3), 0);
	if (mask & 0x00000100)
		put_sample(q, CC_SAMPLE + 8, read_cc_registers_0123(CC_REG4), 0);
	if (mask & 0x00000200)
		put_sample(q, CC_SAMPLE + 9, read_cc_registers_4567(CC_REG4), 0);
	if (mask & 0x00000400)
		put_sample(q, CC_SAMPLE + 10, read_cc_registers_0123(CC_REG5), 0);
	if (mask & 0x00000800)
		put_sample(q, CC_SAMPLE + 11, read_cc_registers_4567(CC_REG5), 0);
	if (mask & 0x00001000)
		put_sample(q, CC_SAMPLE + 12, read_cc_registers_0123(CC_REG6), 0);
	if (mask & 0x00002000)
		put_sample(q, CC_SAMPLE + 13, read_cc_registers_4567(CC_REG6), 0);
	if (mask & 0x00004000)
		put_sample(q, CC_SAMPLE + 14, read_cc_registers_0123(CC_REG7), 0);
	if (mask & 0x00008000)
		put_sample(q, CC_SAMPLE + 15, read_cc_registers_4567(CC_REG7), 0);
	if (mask & 0x00010000)
		put_sample(q, CC_SAMPLE + 16, read_cc_registers_0123(CC_REG8), 0);
	if (mask & 0x00020000)
		put_sample(q, CC_SAMPLE + 17, read_cc_registers_4567(CC_REG8), 0);
	if (mask & 0x00040000)
		put_sample(q, CC_SAMPLE + 18, read_cc_registers_0123(CC_REG9), 0);
	if (mask & 0x00080000)
		put_sample(q, CC_SAMPLE + 19, read_cc_registers_4567(CC_REG9), 0);
	if (mask & 0x00100000)
		put_sample(q, CC_SAMPLE + 20, read_cc_registers_0123(CC_REG10), 0);
	if (mask & 0x00200000)
		put_sample(q, CC_SAMPLE + 21, read_cc_registers_4567(CC_REG10), 0);
	if (mask & 0x00400000)
		put_sample(q, CC_SAMPLE + 22, read_cc_registers_0123(CC_REG11), 0);
	if (mask & 0x00800000)
		put_sample(q, CC_SAMPLE + 23, read_cc_registers_4567(CC_REG11), 0);
	if (mask & 0x01000000)
		put_sample(q, CC_SAMPLE + 24, read_cc_registers_0123(CC_REG12), 0);
	if (mask & 0x02000000)
		put_sample(q, CC_SAMPLE + 24, read_cc_registers_4567(CC_REG12), 0);
	if (mask & 0x04000000)
		put_sample(q, CC_SAMPLE + 26, read_cc_registers_0123(CC_REG13), 0);
	if (mask & 0x08000000)
		put_sample(q, CC_SAMPLE + 27, read_cc_registers_4567(CC_REG13), 0);
	if (mask & 0x10000000)
		put_sample(q, CC_SAMPLE + 28, read_cc_registers_0123(CC_REG14), 0);
	if (mask & 0x20000000)
		put_sample(q, CC_SAMPLE + 29, read_cc_registers_4567(CC_REG14), 0);
	if (mask & 0x40000000)
		put_sample(q, CC_SAMPLE + 30, read_cc_registers_0123(CC_REG15), 0);
	if (mask & 0x80000000)
		put_sample(q, CC_SAMPLE + 31, read_cc_registers_4567(CC_REG15), 0);
	msgrng_flags_restore(flags);
}

static void 
reconfigure(void)
{
	uint32_t cntr_cntrl;

	saved_config = my_perf_area->perf_config;
	num_events = get_num_events(saved_config.events);
	cc_sample = saved_config.cc_sample_rate;

	DPRINT("%d - reconfigure num_events = %d, events = %llx,%llx,%llx,%llx\n",
	    processor_id(), num_events, saved_config.events[0],
	    saved_config.events[1], saved_config.events[2], saved_config.events[3]);

	if (num_events == 0)
		return;

	cntr_cntrl = get_first_control_word(saved_config.flags, saved_config.events);
	write_c0_register(CP0_PERF_COUNTER, PERFCNTRCTL0, cntr_cntrl);
	write_c0_register(CP0_PERF_COUNTER, PERFCNTR0, 0);	/* reset count */
	if (num_events > 1) {
		cntr_cntrl = get_next_control_word(cntr_cntrl, saved_config.events);
		write_c0_register(CP0_PERF_COUNTER, PERFCNTRCTL1, cntr_cntrl);
		write_c0_register(CP0_PERF_COUNTER, PERFCNTR1, 0);	/* reset count */
	}
	saved_timestamp = read_pic_6_timer_count();
}

int xlr_perfmon_no_event_count = 0;
int xlr_perfmon_sample_count;

/* timer callback routine */
void 
xlr_perfmon_sampler(void *dummy)
{
	uint32_t current_ts;
	uint32_t cntr_cntrl = 0;

	/* xlr_ack_interrupt(XLR_PERFMON_IPI_VECTOR); */

	if (my_perf_area->perf_config.magic != PERFMON_ACTIVE_MAGIC)
		return;
	/*
	 * If there has been a change in configuation, update the
	 * configuration
	 */
	if (saved_config.generation != my_perf_area->perf_config.generation) {
		reconfigure();
		return;
	}
	/* credit counter samples if reqd */
	if (saved_config.cc_register_mask && --cc_sample == 0) {
		cc_sample = saved_config.cc_sample_rate;
		do_sample_cc_registers(&my_perf_area->sample_fifo,
		    my_perf_area->perf_config.cc_register_mask);
	}
	if (num_events == 0) {
		xlr_perfmon_no_event_count++;
		return;
	}
	/* put samples in the queue */
	current_ts = read_pic_6_timer_count();
	cntr_cntrl = read_c0_register(CP0_PERF_COUNTER, PERFCNTRCTL0);
	put_sample(&my_perf_area->sample_fifo, make_cpu_tag(cntr_cntrl),
	    read_c0_register(CP0_PERF_COUNTER, PERFCNTR0), current_ts - saved_timestamp);
	xlr_perfmon_sample_count++;
	write_c0_register(CP0_PERF_COUNTER, PERFCNTR0, 0);	/* reset count */

	if (num_events > 1) {
		cntr_cntrl = read_c0_register(CP0_PERF_COUNTER, PERFCNTRCTL1);
		put_sample(&my_perf_area->sample_fifo, make_cpu_tag(cntr_cntrl),
		    read_c0_register(CP0_PERF_COUNTER, PERFCNTR1), current_ts - saved_timestamp);
		xlr_perfmon_sample_count++;
		write_c0_register(CP0_PERF_COUNTER, PERFCNTR1, 0);	/* reset count */

		if (num_events > 2) {
			/* multiplex events */
			cntr_cntrl = get_next_control_word(cntr_cntrl, saved_config.events);
			write_c0_register(CP0_PERF_COUNTER, PERFCNTRCTL0, cntr_cntrl);

			cntr_cntrl = get_next_control_word(cntr_cntrl, saved_config.events);
			write_c0_register(CP0_PERF_COUNTER, PERFCNTRCTL1, cntr_cntrl);
		}
	}
	saved_timestamp = read_pic_6_timer_count();
}

/*
 * Initializes time to gather CPU performance counters and credit counters
 */
void 
xlr_perfmon_init_cpu(void *dummy)
{
	int processor = cpu_ltop_map[PCPU_GET(cpuid)];

	/* run on just one thread per core */
	if (processor % 4)
		return;

	DPRINT("%d : configure with %p", processor, my_perf_area);
	memset(my_perf_area, 0, sizeof(*my_perf_area));
	init_fifo(&my_perf_area->sample_fifo);
	my_perf_area->perf_config.magic = PERFMON_ENABLED_MAGIC;
	my_perf_area->perf_config.generation = PERFMON_INITIAL_GENERATION;
	DPRINT("%d : Initialize", processor);

	return;
}
