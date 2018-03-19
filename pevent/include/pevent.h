/*
 * Copyright (c) 2014-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PEVENT_H
#define PEVENT_H

#include <linux/perf_event.h>

#include "intel-pt.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>


/* A perf event configuration. */
struct pev_config {
	/* The size of the config structure in bytes. */
	size_t size;

	/* The respective field in struct perf_event_attr.
	 *
	 * We require sample_id_all in struct perf_event_attr to be set.
	 */
	uint64_t sample_type;

	/* The respective fields in struct perf_event_mmap_page. */
	uint16_t time_shift;
	uint32_t time_mult;
	uint64_t time_zero;
};

static inline void pev_config_init(struct pev_config *config)
{
	memset(config, 0, sizeof(*config));
	config->size = sizeof(*config);
}


/* The MMAP perf event record. */
struct pev_record_mmap {
	uint32_t pid, tid;
	uint64_t addr;
	uint64_t len;
	uint64_t pgoff;
	char filename[];
};

/* The LOST perf event record. */
struct pev_record_lost {
	uint64_t id;
	uint64_t lost;
};

/* The COMM perf event record. */
struct pev_record_comm {
	uint32_t pid, tid;
	char comm[];
};

/* The EXIT perf event record. */
struct pev_record_exit {
	uint32_t pid, ppid;
	uint32_t tid, ptid;
	uint64_t time;
};

/* The THROTTLE and UNTHROTTLE perf event records. */
struct pev_record_throttle {
	uint64_t time;
	uint64_t id;
	uint64_t stream_id;
};

/* The FORK perf event record. */
struct pev_record_fork {
	uint32_t pid, ppid;
	uint32_t tid, ptid;
	uint64_t time;
};

/* The MMAP2 perf event record. */
struct pev_record_mmap2 {
	uint32_t pid, tid;
	uint64_t addr;
	uint64_t len;
	uint64_t pgoff;
	uint32_t maj, min;
	uint64_t ino;
	uint64_t ino_generation;
	uint32_t prot, flags;
	char filename[];
};

/* The AUX perf event record. */
struct pev_record_aux {
	uint64_t aux_offset;
	uint64_t aux_size;
	uint64_t flags;
};

/* The ITRACE_START perf event record. */
struct pev_record_itrace_start {
	uint32_t pid, tid;
};

/* The LOST_SAMPLES perf event record. */
struct pev_record_lost_samples {
	uint64_t lost;
};

/* The SWITCH_CPU_WIDE perf event record. */
struct pev_record_switch_cpu_wide {
	uint32_t next_prev_pid;
	uint32_t next_prev_tid;
};

/* A perf event record. */
struct pev_event {
	/* The record type (enum perf_event_type). */
	uint32_t type;

	/* The misc field of the perf event header. */
	uint16_t misc;

	/* The perf event record. */
	union {
		/* @type = PERF_RECORD_MMAP. */
		const struct pev_record_mmap *mmap;

		/* @type = PERF_RECORD_LOST. */
		const struct pev_record_lost *lost;

		/* @type = PERF_RECORD_COMM. */
		const struct pev_record_comm *comm;

		/* @type = PERF_RECORD_EXIT. */
		const struct pev_record_exit *exit;

		/* @type = PERF_RECORD_(UN)THROTTLE. */
		const struct pev_record_throttle *throttle;

		/* @type = PERF_RECORD_FORK. */
		const struct pev_record_fork *fork;

		/* @type = PERF_RECORD_MMAP2. */
		const struct pev_record_mmap2 *mmap2;

		/* @type = PERF_RECORD_AUX. */
		const struct pev_record_aux *aux;

		/* @type = PERF_RECORD_ITRACE_START. */
		const struct pev_record_itrace_start *itrace_start;

		/* @type = PERF_RECORD_LOST_SAMPLES. */
		const struct pev_record_lost_samples *lost_samples;

		/* @type = PERF_RECORD_SWITCH_CPU_WIDE. */
		const struct pev_record_switch_cpu_wide *switch_cpu_wide;
	} record;

	/* The additional samples. */
	struct {
		/* The sampled pid and tid. */
		const uint32_t *pid;
		const uint32_t *tid;

		/* The sampled time in perf_event format. */
		const uint64_t *time;

		/* The sampled time in TSC format - if @time is not NULL. */
		uint64_t tsc;

		/* The sampled id. */
		const uint64_t *id;

		/* The sampled stream id. */
		const uint64_t *stream_id;

		/* The sampled cpu. */
		const uint32_t *cpu;

		/* The sample identifier. */
		const uint64_t *identifier;
	} sample;
};

static inline void pev_event_init(struct pev_event *event)
{
	memset(event, 0, sizeof(*event));
}

/* Convert perf_event time to TSC.
 *
 * Converts @time in perf_event format to @tsc.
 *
 * Returns zero on success, a negative error code otherwise.
 * Returns -pte_bad_config if @config->size is too small.
 * Returns -pte_bad_config if @config->time_mult is zero.
 * Returns -pte_internal if @tsc or @config is NULL.
 */
extern int pev_time_to_tsc(uint64_t *tsc, uint64_t time,
			   const struct pev_config *config);

/* Convert TSC to perf_event time.
 *
 * Converts @tsc into @time in perf_event format.
 *
 * Returns zero on success, a negative error code otherwise.
 * Returns -pte_bad_config if @config->size is too small.
 * Returns -pte_bad_config if @config->time_mult is zero.
 * Returns -pte_internal if @time or @config is NULL.
 */
extern int pev_time_from_tsc(uint64_t *time, uint64_t tsc,
			     const struct pev_config *config);

/* Read a perf_event record.
 *
 * Reads one perf_event record from [@begin; @end[ into @event.
 *
 * Returns the number of bytes read on success, a negative error code otherwise.
 * Returns -pte_bad_config if @config->size is too small.
 * Returns -pte_eos if the event does not fit into [@begin; @end[.
 * Returns -pte_internal if @event, @config, @begin, or @end is NULL.
 */
extern int pev_read(struct pev_event *event, const uint8_t *begin,
		    const uint8_t *end, const struct pev_config *config);

/* Write a perf_event record.
 *
 * Writes @event into [@begin; @end[.
 *
 * Returns the number of bytes written on success, a negative error code
 * otherwise.
 * Returns -pte_bad_config if @config->size is too small.
 * Returns -pte_bad_opc if the event type is not known.
 * Returns -pte_eos if the event does not fit into [@begin; @end[.
 * Returns -pte_internal if @begin, @end, @event, or @config is NULL.
 */
extern int pev_write(const struct pev_event *event, uint8_t *begin,
		     uint8_t *end, const struct pev_config *config);

#endif /* PEVENT_H */
