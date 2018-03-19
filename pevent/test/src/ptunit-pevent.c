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

#include "ptunit.h"

#include "pevent.h"


/* A test fixture. */
struct pev_fixture {
	/* A memory buffer. */
	uint8_t buffer[1024];

	/* Two perf events:
	 *
	 *   event[0] is the test setup
	 *   event[1] is the event after writing and reading event[0]
	 */
	struct pev_event event[2];

	/* The perf event configuration. */
	struct pev_config config;

	/* Test samples. */
	struct {
		uint32_t pid, tid;
		uint64_t time;
		uint64_t tsc;
		uint32_t cpu;
	} sample;

	/* The test fixture initialization and finalization functions. */
	struct ptunit_result (*init)(struct pev_fixture *);
	struct ptunit_result (*fini)(struct pev_fixture *);
};

static struct ptunit_result pfix_init(struct pev_fixture *pfix)
{
	memset(pfix->buffer, 0xcd, sizeof(pfix->buffer));
	memset(&pfix->sample, 0xcd, sizeof(pfix->sample));

	pev_event_init(&pfix->event[0]);
	pev_event_init(&pfix->event[1]);

	pev_config_init(&pfix->config);

	return ptu_passed();
}

static struct ptunit_result pfix_init_sample_time(struct pev_fixture *pfix)
{
	ptu_test(pfix_init, pfix);

	pfix->config.sample_type |= (uint64_t) PERF_SAMPLE_TIME;
	pfix->config.time_zero = 0xa0b00000ull;
	pfix->config.time_shift = 4;
	pfix->config.time_mult = 3;

	pfix->sample.time = 0xa0b00cdeull;
	pfix->event[0].sample.time = &pfix->sample.time;

	return ptu_passed();
}

static struct ptunit_result pfix_init_sample_who(struct pev_fixture *pfix)
{
	ptu_test(pfix_init, pfix);

	pfix->config.sample_type |= (uint64_t) PERF_SAMPLE_TID;
	pfix->config.sample_type |= (uint64_t) PERF_SAMPLE_CPU;

	pfix->sample.pid = 0xa0;
	pfix->sample.tid = 0xa1;
	pfix->sample.cpu = 0xb;

	pfix->event[0].sample.pid = &pfix->sample.pid;
	pfix->event[0].sample.tid = &pfix->sample.tid;
	pfix->event[0].sample.cpu = &pfix->sample.cpu;

	return ptu_passed();
}

static struct ptunit_result pfix_read_write(struct pev_fixture *pfix)
{
	uint8_t *begin, *end;
	int size[2];

	begin = pfix->buffer;
	end = begin + sizeof(pfix->buffer);

	size[0] = pev_write(&pfix->event[0], begin, end, &pfix->config);
	ptu_int_gt(size[0], 0);

	size[1] = pev_read(&pfix->event[1], begin, end, &pfix->config);
	ptu_int_gt(size[1], 0);

	ptu_int_eq(size[1], size[0]);

	return ptu_passed();
}

static struct ptunit_result pfix_check_sample_time(struct pev_fixture *pfix)
{
	const uint64_t *time[2];
	uint64_t tsc;
	int errcode;

	time[0] = pfix->event[0].sample.time;
	time[1] = pfix->event[1].sample.time;

	ptu_ptr(time[0]);
	ptu_ptr(time[1]);

	ptu_uint_eq(*time[1], *time[0]);

	errcode = pev_time_to_tsc(&tsc, *time[0], &pfix->config);
	ptu_int_eq(errcode, 0);
	ptu_uint_eq(pfix->event[1].sample.tsc, tsc);

	return ptu_passed();
}

static struct ptunit_result pfix_check_sample_tid(struct pev_fixture *pfix)
{
	const uint32_t *pid[2], *tid[2];

	pid[0] = pfix->event[0].sample.pid;
	pid[1] = pfix->event[1].sample.pid;
	tid[0] = pfix->event[0].sample.tid;
	tid[1] = pfix->event[1].sample.tid;

	ptu_ptr(pid[0]);
	ptu_ptr(pid[1]);
	ptu_ptr(tid[0]);
	ptu_ptr(tid[1]);

	ptu_uint_eq(*pid[1], *pid[0]);
	ptu_uint_eq(*tid[1], *tid[0]);

	return ptu_passed();
}

static struct ptunit_result pfix_check_sample_cpu(struct pev_fixture *pfix)
{
	const uint32_t *cpu[2];

	cpu[0] = pfix->event[0].sample.cpu;
	cpu[1] = pfix->event[1].sample.cpu;

	ptu_ptr(cpu[0]);
	ptu_ptr(cpu[1]);

	ptu_uint_eq(*cpu[1], *cpu[0]);

	return ptu_passed();
}

static struct ptunit_result pfix_check_sample(struct pev_fixture *pfix)
{
	if (pfix->config.sample_type & PERF_SAMPLE_TID)
		ptu_test(pfix_check_sample_tid, pfix);
	else {
		ptu_null(pfix->event[1].sample.pid);
		ptu_null(pfix->event[1].sample.tid);
	}

	if (pfix->config.sample_type & PERF_SAMPLE_TIME)
		ptu_test(pfix_check_sample_time, pfix);
	else
		ptu_null(pfix->event[1].sample.time);

	if (pfix->config.sample_type & PERF_SAMPLE_CPU)
		ptu_test(pfix_check_sample_cpu, pfix);
	else
		ptu_null(pfix->event[1].sample.cpu);

	return ptu_passed();
}

static struct ptunit_result time_to_tsc_null(void)
{
	struct pev_config config;
	uint64_t tsc;
	int errcode;

	errcode = pev_time_to_tsc(NULL, 0x0ull, &config);
	ptu_int_eq(errcode, -pte_internal);

	errcode = pev_time_to_tsc(&tsc, 0x0ull, NULL);
	ptu_int_eq(errcode, -pte_internal);

	return ptu_passed();
}

static struct ptunit_result time_from_tsc_null(void)
{
	struct pev_config config;
	uint64_t time;
	int errcode;

	errcode = pev_time_from_tsc(NULL, 0x0ull, &config);
	ptu_int_eq(errcode, -pte_internal);

	errcode = pev_time_from_tsc(&time, 0x0ull, NULL);
	ptu_int_eq(errcode, -pte_internal);

	return ptu_passed();
}

static struct ptunit_result time_to_tsc(void)
{
	struct pev_config config;
	uint64_t tsc;
	int errcode;

	pev_config_init(&config);
	config.time_shift = 4;
	config.time_mult = 3;
	config.time_zero = 0xa00b00ull;

	errcode = pev_time_to_tsc(&tsc, 0xa00b43ull, &config);
	ptu_int_eq(errcode, 0);
	ptu_uint_eq(tsc, 0x165ull);

	return ptu_passed();
}

static struct ptunit_result time_from_tsc(void)
{
	struct pev_config config;
	uint64_t time;
	int errcode;

	pev_config_init(&config);
	config.time_shift = 4;
	config.time_mult = 3;
	config.time_zero = 0xa00b00ull;

	errcode = pev_time_from_tsc(&time, 0x23bull, &config);
	ptu_int_eq(errcode, 0);
	ptu_uint_eq(time, 0xa00b6bull);

	return ptu_passed();
}

static struct ptunit_result time_to_tsc_bad_config(void)
{
	struct pev_config config;
	uint64_t tsc;
	int errcode;

	memset(&config, 0, sizeof(config));
	config.time_mult = 1;

	errcode = pev_time_to_tsc(&tsc, 0x0ull, &config);
	ptu_int_eq(errcode, -pte_bad_config);

	config.size = sizeof(config);
	config.time_mult = 0;

	errcode = pev_time_to_tsc(&tsc, 0x0ull, &config);
	ptu_int_eq(errcode, -pte_bad_config);

	return ptu_passed();
}

static struct ptunit_result time_from_tsc_bad_config(void)
{
	struct pev_config config;
	uint64_t time;
	int errcode;

	memset(&config, 0, sizeof(config));
	config.time_mult = 1;

	errcode = pev_time_from_tsc(&time, 0x0ull, &config);
	ptu_int_eq(errcode, -pte_bad_config);

	config.size = sizeof(config);
	config.time_mult = 0;

	errcode = pev_time_from_tsc(&time, 0x0ull, &config);
	ptu_int_eq(errcode, -pte_bad_config);

	return ptu_passed();
}

static struct ptunit_result read_bad_config(void)
{
	union {
		struct perf_event_header header;
		uint8_t buffer[128];
	} input;
	struct pev_config config;
	struct pev_event event;
	int errcode;

	memset(input.buffer, 0, sizeof(input.buffer));
	input.header.type = PERF_RECORD_ITRACE_START;
	input.header.size = sizeof(event.record.itrace_start) + 0x8;

	memset(&config, 0, sizeof(config));
	config.sample_type |= (uint64_t) PERF_SAMPLE_CPU;

	errcode = pev_read(&event, input.buffer,
			   input.buffer + sizeof(input.buffer), &config);
	ptu_int_eq(errcode, -pte_bad_config);

	return ptu_passed();
}

static struct ptunit_result write_bad_config(void)
{
	struct pev_record_itrace_start itrace_start;
	struct pev_config config;
	struct pev_event event;
	uint32_t cpu;
	uint8_t buffer[128];
	int errcode;

	memset(&itrace_start, 0, sizeof(itrace_start));

	pev_event_init(&event);
	event.type = PERF_RECORD_ITRACE_START;
	event.record.itrace_start = &itrace_start;
	event.sample.cpu = &cpu;

	memset(&config, 0, sizeof(config));
	config.sample_type |= (uint64_t) PERF_SAMPLE_CPU;

	errcode = pev_write(&event, buffer, buffer + sizeof(buffer), &config);
	ptu_int_eq(errcode, -pte_bad_config);

	return ptu_passed();
}

static struct ptunit_result bad_string(uint16_t type)
{
	union {
		struct perf_event_header header;
		uint8_t buffer[512];
	} input;

	struct pev_config config;
	struct pev_event event;
	int errcode;

	pev_config_init(&config);

	memset(input.buffer, 0xcc, sizeof(input.buffer));
	input.header.type = type;
	input.header.misc = 0;
	input.header.size = 0x50;

	errcode = pev_read(&event, input.buffer,
			   input.buffer + sizeof(input.buffer), &config);
	ptu_int_eq(errcode, -pte_bad_packet);

	return ptu_passed();
}

static struct ptunit_result mmap(struct pev_fixture *pfix)
{
	union {
		struct pev_record_mmap record;
		char buffer[1024];
	} mmap;

	mmap.record.pid = 0xa;
	mmap.record.tid = 0xb;
	mmap.record.addr = 0xa00100ull;
	mmap.record.len = 0x110ull;
	mmap.record.pgoff = 0xb0000ull;
	strcpy(mmap.record.filename, "foo.so");

	pfix->event[0].record.mmap = &mmap.record;
	pfix->event[0].type = PERF_RECORD_MMAP;

	ptu_test(pfix_read_write, pfix);
	ptu_test(pfix_check_sample, pfix);

	ptu_int_eq(pfix->event[1].type, pfix->event[0].type);
	ptu_ptr(pfix->event[1].record.mmap);
	ptu_uint_eq(pfix->event[1].record.mmap->pid, mmap.record.pid);
	ptu_uint_eq(pfix->event[1].record.mmap->tid, mmap.record.tid);
	ptu_uint_eq(pfix->event[1].record.mmap->addr, mmap.record.addr);
	ptu_uint_eq(pfix->event[1].record.mmap->len, mmap.record.len);
	ptu_uint_eq(pfix->event[1].record.mmap->pgoff, mmap.record.pgoff);
	ptu_str_eq(pfix->event[1].record.mmap->filename, mmap.record.filename);

	return ptu_passed();
}

static struct ptunit_result lost(struct pev_fixture *pfix)
{
	struct pev_record_lost lost;

	lost.id = 0xa042ull;
	lost.lost = 0xeull;

	pfix->event[0].record.lost = &lost;
	pfix->event[0].type = PERF_RECORD_LOST;

	ptu_test(pfix_read_write, pfix);
	ptu_test(pfix_check_sample, pfix);

	ptu_int_eq(pfix->event[1].type, pfix->event[0].type);
	ptu_ptr(pfix->event[1].record.lost);
	ptu_uint_eq(pfix->event[1].record.lost->id, lost.id);
	ptu_uint_eq(pfix->event[1].record.lost->lost, lost.lost);

	return ptu_passed();
}

static struct ptunit_result comm(struct pev_fixture *pfix)
{
	union {
		struct pev_record_comm record;
		char buffer[1024];
	} comm;

	comm.record.pid = 0xa;
	comm.record.tid = 0xb;
	strcpy(comm.record.comm, "foo -b ar");

	pfix->event[0].record.comm = &comm.record;
	pfix->event[0].type = PERF_RECORD_COMM;

	ptu_test(pfix_read_write, pfix);
	ptu_test(pfix_check_sample, pfix);

	ptu_int_eq(pfix->event[1].type, pfix->event[0].type);
	ptu_ptr(pfix->event[1].record.comm);
	ptu_uint_eq(pfix->event[1].record.comm->pid, comm.record.pid);
	ptu_uint_eq(pfix->event[1].record.comm->tid, comm.record.tid);
	ptu_str_eq(pfix->event[1].record.comm->comm, comm.record.comm);

	return ptu_passed();
}

static struct ptunit_result record_exit(struct pev_fixture *pfix)
{
	struct pev_record_exit exit;

	exit.pid = 0xa;
	exit.ppid = 0xaa;
	exit.tid = 0xb;
	exit.ptid = 0xab;
	exit.time = 0xabcdefull;

	pfix->event[0].record.exit = &exit;
	pfix->event[0].type = PERF_RECORD_EXIT;

	ptu_test(pfix_read_write, pfix);
	ptu_test(pfix_check_sample, pfix);

	ptu_int_eq(pfix->event[1].type, pfix->event[0].type);
	ptu_ptr(pfix->event[1].record.exit);
	ptu_uint_eq(pfix->event[1].record.exit->pid, exit.pid);
	ptu_uint_eq(pfix->event[1].record.exit->ppid, exit.ppid);
	ptu_uint_eq(pfix->event[1].record.exit->tid, exit.tid);
	ptu_uint_eq(pfix->event[1].record.exit->ptid, exit.ptid);
	ptu_uint_eq(pfix->event[1].record.exit->time, exit.time);

	return ptu_passed();
}

static struct ptunit_result throttle(struct pev_fixture *pfix)
{
	struct pev_record_throttle throttle;

	throttle.time = 0xabcdeull;
	throttle.id = 0xa042ull;
	throttle.stream_id = 0xb00ull;

	pfix->event[0].record.throttle = &throttle;
	pfix->event[0].type = PERF_RECORD_THROTTLE;

	ptu_test(pfix_read_write, pfix);
	ptu_test(pfix_check_sample, pfix);

	ptu_int_eq(pfix->event[1].type, pfix->event[0].type);
	ptu_ptr(pfix->event[1].record.throttle);
	ptu_uint_eq(pfix->event[1].record.throttle->time, throttle.time);
	ptu_uint_eq(pfix->event[1].record.throttle->id, throttle.id);
	ptu_uint_eq(pfix->event[1].record.throttle->stream_id,
		    throttle.stream_id);

	return ptu_passed();
}

static struct ptunit_result unthrottle(struct pev_fixture *pfix)
{
	struct pev_record_throttle throttle;

	throttle.time = 0xc00042ull;
	throttle.id = 0x23ull;
	throttle.stream_id = 0x0ull;

	pfix->event[0].record.throttle = &throttle;
	pfix->event[0].type = PERF_RECORD_UNTHROTTLE;

	ptu_test(pfix_read_write, pfix);
	ptu_test(pfix_check_sample, pfix);

	ptu_int_eq(pfix->event[1].type, pfix->event[0].type);
	ptu_ptr(pfix->event[1].record.throttle);
	ptu_uint_eq(pfix->event[1].record.throttle->time, throttle.time);
	ptu_uint_eq(pfix->event[1].record.throttle->id, throttle.id);
	ptu_uint_eq(pfix->event[1].record.throttle->stream_id,
		    throttle.stream_id);

	return ptu_passed();
}

static struct ptunit_result fork(struct pev_fixture *pfix)
{
	struct pev_record_fork fork;

	fork.pid = 0xa;
	fork.ppid = 0xaa;
	fork.tid = 0xb;
	fork.ptid = 0xab;
	fork.time = 0xabcdefull;

	pfix->event[0].record.fork = &fork;
	pfix->event[0].type = PERF_RECORD_FORK;

	ptu_test(pfix_read_write, pfix);
	ptu_test(pfix_check_sample, pfix);

	ptu_int_eq(pfix->event[1].type, pfix->event[0].type);
	ptu_ptr(pfix->event[1].record.fork);
	ptu_uint_eq(pfix->event[1].record.fork->pid, fork.pid);
	ptu_uint_eq(pfix->event[1].record.fork->ppid, fork.ppid);
	ptu_uint_eq(pfix->event[1].record.fork->tid, fork.tid);
	ptu_uint_eq(pfix->event[1].record.fork->ptid, fork.ptid);
	ptu_uint_eq(pfix->event[1].record.fork->time, fork.time);

	return ptu_passed();
}

static struct ptunit_result mmap2(struct pev_fixture *pfix)
{
	union {
		struct pev_record_mmap2 record;
		char buffer[1024];
	} mmap2;

	mmap2.record.pid = 0xa;
	mmap2.record.tid = 0xb;
	mmap2.record.addr = 0xa00100ull;
	mmap2.record.len = 0x110ull;
	mmap2.record.pgoff = 0xb0000ull;
	mmap2.record.maj = 7;
	mmap2.record.min = 2;
	mmap2.record.ino = 0x8080ull;
	mmap2.record.ino_generation = 0x4ull;
	mmap2.record.prot = 0x755;
	mmap2.record.flags = 0;
	strcpy(mmap2.record.filename, "foo.so");

	pfix->event[0].record.mmap2 = &mmap2.record;
	pfix->event[0].type = PERF_RECORD_MMAP2;

	ptu_test(pfix_read_write, pfix);
	ptu_test(pfix_check_sample, pfix);

	ptu_int_eq(pfix->event[1].type, pfix->event[0].type);
	ptu_ptr(pfix->event[1].record.mmap2);
	ptu_uint_eq(pfix->event[1].record.mmap2->pid, mmap2.record.pid);
	ptu_uint_eq(pfix->event[1].record.mmap2->tid, mmap2.record.tid);
	ptu_uint_eq(pfix->event[1].record.mmap2->addr, mmap2.record.addr);
	ptu_uint_eq(pfix->event[1].record.mmap2->len, mmap2.record.len);
	ptu_uint_eq(pfix->event[1].record.mmap2->pgoff, mmap2.record.pgoff);
	ptu_uint_eq(pfix->event[1].record.mmap2->maj, mmap2.record.maj);
	ptu_uint_eq(pfix->event[1].record.mmap2->min, mmap2.record.min);
	ptu_uint_eq(pfix->event[1].record.mmap2->ino, mmap2.record.ino);
	ptu_uint_eq(pfix->event[1].record.mmap2->ino_generation,
		    mmap2.record.ino_generation);
	ptu_uint_eq(pfix->event[1].record.mmap2->prot, mmap2.record.prot);
	ptu_uint_eq(pfix->event[1].record.mmap2->flags, mmap2.record.flags);
	ptu_str_eq(pfix->event[1].record.mmap2->filename,
		   mmap2.record.filename);

	return ptu_passed();
}

static struct ptunit_result aux(struct pev_fixture *pfix)
{
	struct pev_record_aux aux;

	aux.aux_offset = 0xc00042ull;
	aux.aux_size = 0x23ull;
	aux.flags = 0x0ull;

	pfix->event[0].record.aux = &aux;
	pfix->event[0].type = PERF_RECORD_AUX;

	ptu_test(pfix_read_write, pfix);
	ptu_test(pfix_check_sample, pfix);

	ptu_int_eq(pfix->event[1].type, pfix->event[0].type);
	ptu_ptr(pfix->event[1].record.aux);
	ptu_uint_eq(pfix->event[1].record.aux->aux_offset, aux.aux_offset);
	ptu_uint_eq(pfix->event[1].record.aux->aux_size, aux.aux_size);
	ptu_uint_eq(pfix->event[1].record.aux->flags, aux.flags);

	return ptu_passed();
}

static struct ptunit_result itrace_start(struct pev_fixture *pfix)
{
	struct pev_record_itrace_start itrace_start;

	itrace_start.pid = 0xa;
	itrace_start.tid = 0xb;

	pfix->event[0].record.itrace_start = &itrace_start;
	pfix->event[0].type = PERF_RECORD_ITRACE_START;

	ptu_test(pfix_read_write, pfix);
	ptu_test(pfix_check_sample, pfix);

	ptu_int_eq(pfix->event[1].type, pfix->event[0].type);
	ptu_ptr(pfix->event[1].record.itrace_start);
	ptu_uint_eq(pfix->event[1].record.itrace_start->pid, itrace_start.pid);
	ptu_uint_eq(pfix->event[1].record.itrace_start->tid, itrace_start.tid);

	return ptu_passed();
}

static struct ptunit_result lost_samples(struct pev_fixture *pfix)
{
	struct pev_record_lost_samples lost_samples;

	lost_samples.lost = 0xc00042ull;

	pfix->event[0].record.lost_samples = &lost_samples;
	pfix->event[0].type = PERF_RECORD_LOST_SAMPLES;

	ptu_test(pfix_read_write, pfix);
	ptu_test(pfix_check_sample, pfix);

	ptu_int_eq(pfix->event[1].type, pfix->event[0].type);
	ptu_ptr(pfix->event[1].record.lost_samples);
	ptu_uint_eq(pfix->event[1].record.lost_samples->lost,
		    lost_samples.lost);

	return ptu_passed();
}

static struct ptunit_result switch_task(struct pev_fixture *pfix,
					int switch_out)
{
	pfix->event[0].type = PERF_RECORD_SWITCH;
	pfix->event[0].misc = switch_out ? PERF_RECORD_MISC_SWITCH_OUT : 0;

	ptu_test(pfix_read_write, pfix);
	ptu_test(pfix_check_sample, pfix);

	ptu_int_eq(pfix->event[1].type, pfix->event[0].type);
	ptu_int_eq(pfix->event[1].misc, pfix->event[0].misc);

	return ptu_passed();
}

static struct ptunit_result switch_cpu_wide(struct pev_fixture *pfix,
					    int switch_out)
{
	struct pev_record_switch_cpu_wide switch_cpu_wide;

	switch_cpu_wide.next_prev_pid = 0xa;
	switch_cpu_wide.next_prev_tid = 0xb;

	pfix->event[0].record.switch_cpu_wide = &switch_cpu_wide;
	pfix->event[0].type = PERF_RECORD_SWITCH_CPU_WIDE;
	pfix->event[0].misc = switch_out ? PERF_RECORD_MISC_SWITCH_OUT : 0;

	ptu_test(pfix_read_write, pfix);
	ptu_test(pfix_check_sample, pfix);

	ptu_int_eq(pfix->event[1].type, pfix->event[0].type);
	ptu_int_eq(pfix->event[1].misc, pfix->event[0].misc);
	ptu_ptr(pfix->event[1].record.switch_cpu_wide);
	ptu_uint_eq(pfix->event[1].record.switch_cpu_wide->next_prev_pid,
		    switch_cpu_wide.next_prev_pid);
	ptu_uint_eq(pfix->event[1].record.switch_cpu_wide->next_prev_tid,
		    switch_cpu_wide.next_prev_tid);

	return ptu_passed();
}

int main(int argc, char **argv)
{
	struct pev_fixture pfix, pfix_time, pfix_who;
	struct ptunit_suite suite;

	pfix.init = pfix_init;
	pfix.fini = NULL;

	pfix_time.init = pfix_init_sample_time;
	pfix_time.fini = NULL;

	pfix_who.init = pfix_init_sample_who;
	pfix_who.fini = NULL;

	suite = ptunit_mk_suite(argc, argv);

	ptu_run(suite, time_to_tsc_null);
	ptu_run(suite, time_from_tsc_null);

	ptu_run(suite, time_to_tsc);
	ptu_run(suite, time_from_tsc);

	ptu_run(suite, time_to_tsc_bad_config);
	ptu_run(suite, time_from_tsc_bad_config);
	ptu_run(suite, read_bad_config);
	ptu_run(suite, write_bad_config);

	ptu_run_p(suite, bad_string, PERF_RECORD_MMAP);
	ptu_run_p(suite, bad_string, PERF_RECORD_COMM);
	ptu_run_p(suite, bad_string, PERF_RECORD_MMAP2);

	ptu_run_f(suite, mmap, pfix);
	ptu_run_f(suite, lost, pfix);
	ptu_run_f(suite, comm, pfix);
	ptu_run_f(suite, record_exit, pfix);
	ptu_run_f(suite, throttle, pfix);
	ptu_run_f(suite, unthrottle, pfix);
	ptu_run_f(suite, fork, pfix);
	ptu_run_f(suite, mmap2, pfix);
	ptu_run_f(suite, aux, pfix);
	ptu_run_f(suite, itrace_start, pfix);
	ptu_run_f(suite, lost_samples, pfix);
	ptu_run_fp(suite, switch_task, pfix, 0);
	ptu_run_fp(suite, switch_task, pfix, 1);
	ptu_run_fp(suite, switch_cpu_wide, pfix, 0);
	ptu_run_fp(suite, switch_cpu_wide, pfix, 1);

	ptu_run_f(suite, mmap, pfix_time);
	ptu_run_f(suite, lost, pfix_time);
	ptu_run_f(suite, comm, pfix_time);
	ptu_run_f(suite, record_exit, pfix_time);
	ptu_run_f(suite, throttle, pfix_time);
	ptu_run_f(suite, unthrottle, pfix_time);
	ptu_run_f(suite, fork, pfix_time);
	ptu_run_f(suite, mmap2, pfix_time);
	ptu_run_f(suite, aux, pfix_time);
	ptu_run_f(suite, itrace_start, pfix_time);
	ptu_run_f(suite, lost_samples, pfix_time);
	ptu_run_fp(suite, switch_task, pfix_time, 0);
	ptu_run_fp(suite, switch_task, pfix_time, 1);
	ptu_run_fp(suite, switch_cpu_wide, pfix_time, 0);
	ptu_run_fp(suite, switch_cpu_wide, pfix_time, 1);

	ptu_run_f(suite, mmap, pfix_who);
	ptu_run_f(suite, lost, pfix_who);
	ptu_run_f(suite, comm, pfix_who);
	ptu_run_f(suite, record_exit, pfix_who);
	ptu_run_f(suite, throttle, pfix_who);
	ptu_run_f(suite, unthrottle, pfix_who);
	ptu_run_f(suite, fork, pfix_who);
	ptu_run_f(suite, mmap2, pfix_who);
	ptu_run_f(suite, aux, pfix_who);
	ptu_run_f(suite, itrace_start, pfix_who);
	ptu_run_f(suite, lost_samples, pfix_who);
	ptu_run_fp(suite, switch_task, pfix_who, 0);
	ptu_run_fp(suite, switch_task, pfix_who, 1);
	ptu_run_fp(suite, switch_cpu_wide, pfix_who, 0);
	ptu_run_fp(suite, switch_cpu_wide, pfix_who, 1);

	return ptunit_report(&suite);
}
