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

#ifndef PERFMON_H
#define PERFMON_H

#include <mips/rmi/perfmon_xlrconfig.h>

/*
 * category events reported by the perfmon library
 */
enum event_category_t {
	PERF_CP0_COUNTER = 1, PERF_CP2_CREDITS, PERF_L2_COUNTER,
	PERF_SBC_COUNTER, PERF_SBC_CREDITS, PERF_GMAC0_COUNTER, PERF_GMAC1_COUNTER,
	PERF_GMAC2_COUNTER, PERF_GMAC_STAT_COM, PERF_GMAC_STAT_TX,
PERF_GMAC_STAT_RX, PERF_DRAM_COUNTER, PERF_PARAMETER_CONF = 127};


enum perf_param_t {
	PERF_CPU_SAMPLING_INTERVAL, PERF_SYS_SAMPLING_INTERVAL, PERF_CC_SAMPLE_RATE, PERF_CP0_FLAGS
};

#define CPO_EVENTS_TEMPLATE  0x06	/* enable kernel and user events */

#define PERFMON_ACTIVE_MAGIC 0xc001
#define PERFMON_ENABLED_MAGIC 0xb007
#define PERFMON_INITIAL_GENERATION 0x0101

#define PERFMON_SERVER_PORT 7007

enum system_bridge_credits_t {
	PCIX_CREDITS, HT_CREDITS, GIO_CREDITS, OTHER_CREDITS
};

struct perf_config_data {
	uint16_t magic;		/* monitor start when this is initialized */
	uint16_t generation;	/* incremented when the config changes */
	uint16_t flags;
	uint16_t cc_sample_rate;/* rate at which credit counters are sampled
				 * relative to sampling_rate */
	uint32_t sampling_rate;	/* rate at which events are sampled */
	uint32_t cc_register_mask;	/* credit counters registers to be
					 * sampled */
	uint64_t events[NTHREADS];	/* events bitmap for each thread */
};

struct perf_sample {
	uint32_t counter;
	uint32_t timestamp;
	uint32_t sample_tag;
	uint32_t duration;
};

struct sample_q {
	int32_t head, tail;
	struct perf_sample samples[PERF_SAMPLE_BUFSZ];
	uint32_t overflows;
};

struct perf_area {
	struct perf_config_data perf_config;
	struct sample_q sample_fifo;
};

/*
 * We have a shared location to keep a global tick counter for all the
 * CPUS  - TODO is this optimal? effect on cache?
 */
extern uint32_t *xlr_perfmon_timer_loc;

#define PERFMON_TIMESTAMP_LOC (xlr_perfmon_timer_loc)

static __inline__ uint32_t 
perfmon_timestamp_get(void)
{
	return *PERFMON_TIMESTAMP_LOC;
}

static __inline__ void 
perfmon_timestamp_set(uint32_t val)
{
	*PERFMON_TIMESTAMP_LOC = val;
}

static __inline__ void 
perfmon_timestamp_incr(int val)
{
	(*PERFMON_TIMESTAMP_LOC) += val;
}

static __inline__ void 
send_sample_gts(uint32_t tag, uint32_t value, uint32_t td)
{
	xlr_send_sample(tag, value, perfmon_timestamp_get(), td);
}

/*
 * Simple FIFO, one producer - one consumer - circlar queue - no locking
 */

static __inline__ void 
init_fifo(struct sample_q *q)
{
	q->head = q->tail = 0;
}

static __inline__ void 
put_sample(struct sample_q *q, uint32_t sample_tag, uint32_t counter,
    uint32_t duration)
{
	uint32_t timestamp = perfmon_timestamp_get();
	int new_tail = (q->tail + 1) % PERF_SAMPLE_BUFSZ;

	if (q->head == new_tail) {
		q->overflows++;
		return;
	}
	q->samples[new_tail].sample_tag = sample_tag;
	q->samples[new_tail].counter = counter;
	q->samples[new_tail].timestamp = timestamp;
	q->samples[new_tail].duration = duration;

	q->tail = new_tail;
}

static __inline__ int 
get_sample(struct sample_q *q, uint32_t * sample_tag, uint32_t * counter,
    uint32_t * timestamp, uint32_t * duration)
{
	int head = q->head;

	if (head == q->tail)
		return 0;
	*sample_tag = q->samples[head].sample_tag;
	*counter = q->samples[head].counter;
	*timestamp = q->samples[head].timestamp;
	*duration = q->samples[head].duration;

	q->head = (head + 1) % PERF_SAMPLE_BUFSZ;
	return 1;
}

static __inline__ void 
clear_queue(struct sample_q *q)
{
	q->head = q->tail;
}
void xlr_perfmon_init_cpu(void *);
void xlr_perfmon_sampler(void *);
void log_active_core(int core);
int get_start_generation(void);

void xlr_perfmon_clockhandler(void);
extern int xlr_perfmon_started;

#endif				/* PERFMON_H */
