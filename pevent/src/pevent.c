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

#include "pevent.h"


#define pev_config_has(config, field) \
	(config->size >= (offsetof(struct pev_config, field) + \
			  sizeof(config->field)))

int pev_time_to_tsc(uint64_t *tsc, uint64_t time,
		    const struct pev_config *config)
{
	uint64_t quot, rem, time_zero;
	uint16_t time_shift;
	uint32_t time_mult;

	if (!tsc || !config)
		return -pte_internal;

	if (!pev_config_has(config, time_zero))
		return -pte_bad_config;

	time_shift = config->time_shift;
	time_mult = config->time_mult;
	time_zero = config->time_zero;

	if (!time_mult)
		return -pte_bad_config;

	time -= time_zero;

	quot = time / time_mult;
	rem = time % time_mult;

	quot <<= time_shift;
	rem <<= time_shift;
	rem /= time_mult;

	*tsc = quot + rem;

	return 0;
}

int pev_time_from_tsc(uint64_t *time, uint64_t tsc,
		      const struct pev_config *config)
{
	uint64_t quot, rem, time_zero;
	uint16_t time_shift;
	uint32_t time_mult;

	if (!time || !config)
		return -pte_internal;

	if (!pev_config_has(config, time_zero))
		return -pte_bad_config;

	time_shift = config->time_shift;
	time_mult = config->time_mult;
	time_zero = config->time_zero;

	if (!time_mult)
		return -pte_bad_config;

	quot = tsc >> time_shift;
	rem = tsc & ((1ull << time_shift) - 1);

	quot *= time_mult;
	rem *= time_mult;
	rem >>= time_shift;

	*time = time_zero + quot + rem;

	return 0;
}

static int pev_strlen(const char *begin, const void *end_arg)
{
	const char *pos, *end;

	if (!begin || !end_arg)
		return -pte_internal;

	end = (const char *) end_arg;
	if (end < begin)
		return -pte_internal;

	for (pos = begin; pos < end; ++pos) {
		if (!pos[0])
			return (int) (pos - begin) + 1;
	}

	return -pte_bad_packet;
}

static int pev_read_samples(struct pev_event *event, const uint8_t *begin,
			    const uint8_t *end, const struct pev_config *config)
{
	const uint8_t *pos;
	uint64_t sample_type;

	if (!event || !begin || !config)
		return -pte_internal;

	if (!pev_config_has(config, sample_type))
		return -pte_bad_config;

	sample_type = config->sample_type;
	pos = begin;

	if (sample_type & PERF_SAMPLE_TID) {
		event->sample.pid = (const uint32_t *) &pos[0];
		event->sample.tid = (const uint32_t *) &pos[4];
		pos += 8;
	}

	if (sample_type & PERF_SAMPLE_TIME) {
		int errcode;

		event->sample.time = (const uint64_t *) pos;
		pos += 8;

		/* We're reading the time.  Let's make sure the pointer lies
		 * inside the buffer.
		 */
		if (end < pos)
			return -pte_nosync;

		errcode = pev_time_to_tsc(&event->sample.tsc,
					  *event->sample.time, config);
		if (errcode < 0)
			return errcode;
	}

	if (sample_type & PERF_SAMPLE_ID) {
		event->sample.id = (const uint64_t *) pos;
		pos += 8;
	}

	if (sample_type & PERF_SAMPLE_STREAM_ID) {
		event->sample.stream_id = (const uint64_t *) pos;
		pos += 8;
	}

	if (sample_type & PERF_SAMPLE_CPU) {
		event->sample.cpu = (const uint32_t *) pos;
		pos += 8;
	}

	if (sample_type & PERF_SAMPLE_IDENTIFIER) {
		event->sample.identifier = (const uint64_t *) pos;
		pos += 8;
	}

	return (int) (pos - begin);
}

int pev_read(struct pev_event *event, const uint8_t *begin, const uint8_t *end,
	     const struct pev_config *config)
{
	const struct perf_event_header *header;
	const uint8_t *pos;
	int size;

	if (!event || !begin || end < begin)
		return -pte_internal;

	pos = begin;
	if (end < (pos + sizeof(*header)))
		return -pte_eos;

	header = (const struct perf_event_header *) pos;
	pos += sizeof(*header);

	if (!header->type || (end < (begin + header->size)))
		return -pte_eos;

	/* Stay within the packet. */
	end = begin + header->size;

	memset(event, 0, sizeof(*event));

	event->type = header->type;
	event->misc = header->misc;

	switch (event->type) {
	default:
		/* We don't provide samples.
		 *
		 * It would be possible since we know the event's total size
		 * as well as the sample size.  But why?
		 */
		return (int) header->size;

	case PERF_RECORD_MMAP: {
		int slen;

		event->record.mmap = (const struct pev_record_mmap *) pos;

		slen = pev_strlen(event->record.mmap->filename, end);
		if (slen < 0)
			return slen;

		slen = (slen + 7) & ~7;

		pos += sizeof(*event->record.mmap);
		pos += slen;
	}
		break;

	case PERF_RECORD_LOST:
		event->record.lost = (const struct pev_record_lost *) pos;
		pos += sizeof(*event->record.lost);
		break;

	case PERF_RECORD_COMM: {
		int slen;

		event->record.comm = (const struct pev_record_comm *) pos;

		slen = pev_strlen(event->record.comm->comm, end);
		if (slen < 0)
			return slen;

		slen = (slen + 7) & ~7;

		pos += sizeof(*event->record.comm);
		pos += slen;
	}
		break;

	case PERF_RECORD_EXIT:
		event->record.exit = (const struct pev_record_exit *) pos;
		pos += sizeof(*event->record.exit);
		break;

	case PERF_RECORD_THROTTLE:
	case PERF_RECORD_UNTHROTTLE:
		event->record.throttle =
			(const struct pev_record_throttle *) pos;
		pos += sizeof(*event->record.throttle);
		break;

	case PERF_RECORD_FORK:
		event->record.fork = (const struct pev_record_fork *) pos;
		pos += sizeof(*event->record.fork);
		break;

	case PERF_RECORD_MMAP2: {
		int slen;

		event->record.mmap2 = (const struct pev_record_mmap2 *) pos;

		slen = pev_strlen(event->record.mmap2->filename, end);
		if (slen < 0)
			return slen;

		slen = (slen + 7) & ~7;

		pos += sizeof(*event->record.mmap2);
		pos += slen;
	}
		break;

	case PERF_RECORD_AUX:
		event->record.aux = (const struct pev_record_aux *) pos;
		pos += sizeof(*event->record.aux);
		break;

	case PERF_RECORD_ITRACE_START:
		event->record.itrace_start =
			(const struct pev_record_itrace_start *) pos;
		pos += sizeof(*event->record.itrace_start);
		break;

	case PERF_RECORD_LOST_SAMPLES:
		event->record.lost_samples =
			(const struct pev_record_lost_samples *) pos;
		pos += sizeof(*event->record.lost_samples);
		break;

	case PERF_RECORD_SWITCH:
		break;

	case PERF_RECORD_SWITCH_CPU_WIDE:
		event->record.switch_cpu_wide =
			(const struct pev_record_switch_cpu_wide *) pos;
		pos += sizeof(*event->record.switch_cpu_wide);
		break;
	}

	size = pev_read_samples(event, pos, end, config);
	if (size < 0)
		return size;

	pos += size;
	if (pos < begin)
		return -pte_internal;

	size = (int) (pos - begin);
	if ((uint16_t) size != header->size)
		return -pte_nosync;

	return size;
}

static size_t sample_size(const struct pev_event *event)
{
	size_t size;

	if (!event)
		return 0;

	size = 0;

	if (event->sample.tid) {
		size += sizeof(*event->sample.pid);
		size += sizeof(*event->sample.tid);
	}

	if (event->sample.time)
		size += sizeof(*event->sample.time);

	if (event->sample.id)
		size += sizeof(*event->sample.id);

	if (event->sample.stream_id)
		size += sizeof(*event->sample.stream_id);

	if (event->sample.cpu) {
		size += sizeof(*event->sample.cpu);
		size += sizeof(uint32_t);
	}

	if (event->sample.identifier)
		size += sizeof(*event->sample.identifier);

	return size;
}

static void write(uint8_t **stream, const void *object, size_t size)
{
	memcpy(*stream, object, size);
	*stream += size;
}

static void clear(uint8_t **stream, size_t size)
{
	memset(*stream, 0, size);
	*stream += size;
}

static int write_samples(uint8_t **stream, const struct pev_event *event,
			 const struct pev_config *config)
{
	uint64_t sample_type;

	if (!event || !config)
		return -pte_internal;

	if (!pev_config_has(config, sample_type))
		return -pte_bad_config;

	sample_type = config->sample_type;

	if (sample_type & PERF_SAMPLE_TID) {
		sample_type &= ~(uint64_t) PERF_SAMPLE_TID;

		if (!event->sample.pid || !event->sample.tid)
			return -pte_bad_packet;

		write(stream, event->sample.pid, sizeof(*event->sample.pid));
		write(stream, event->sample.tid, sizeof(*event->sample.tid));
	}

	if (sample_type & PERF_SAMPLE_TIME) {
		sample_type &= ~(uint64_t) PERF_SAMPLE_TIME;

		if (!event->sample.time)
			return -pte_bad_packet;

		write(stream, event->sample.time, sizeof(*event->sample.time));
	}

	if (sample_type & PERF_SAMPLE_ID) {
		sample_type &= ~(uint64_t) PERF_SAMPLE_ID;

		if (!event->sample.id)
			return -pte_bad_packet;

		write(stream, event->sample.id, sizeof(*event->sample.id));
	}

	if (sample_type & PERF_SAMPLE_STREAM_ID) {
		sample_type &= ~(uint64_t) PERF_SAMPLE_STREAM_ID;

		if (!event->sample.stream_id)
			return -pte_bad_packet;

		write(stream, event->sample.stream_id,
		      sizeof(*event->sample.stream_id));
	}

	if (sample_type & PERF_SAMPLE_CPU) {
		sample_type &= ~(uint64_t) PERF_SAMPLE_CPU;

		if (!event->sample.cpu)
			return -pte_bad_packet;

		write(stream, event->sample.cpu, sizeof(*event->sample.cpu));
		*stream += sizeof(uint32_t);
	}

	if (sample_type & PERF_SAMPLE_IDENTIFIER) {
		sample_type &= ~(uint64_t) PERF_SAMPLE_IDENTIFIER;

		if (!event->sample.identifier)
			return -pte_bad_packet;

		write(stream, event->sample.identifier,
		      sizeof(*event->sample.identifier));
	}

	if (sample_type)
		return -pte_bad_packet;

	return 0;
}

int pev_write(const struct pev_event *event, uint8_t *begin, uint8_t *end,
	      const struct pev_config *config)
{
	struct perf_event_header header;
	uint8_t *pos;
	size_t size;
	int errcode;

	if (!event || !begin || end < begin)
		return -pte_internal;

	pos = begin;
	size = sizeof(header) + sample_size(event);
	if (UINT16_MAX < size)
		return -pte_internal;

	header.type = event->type;
	header.misc = event->misc;

	switch (header.type) {
	default:
		return -pte_bad_opc;

	case PERF_RECORD_MMAP: {
		size_t slen, gap;

		slen = strlen(event->record.mmap->filename) + 1;
		gap = ((slen + 7) & ~7) - slen;

		size += sizeof(*event->record.mmap) + slen + gap;
		if (UINT16_MAX < size)
			return -pte_bad_packet;

		header.size = (uint16_t) size;
		if (end < pos + header.size)
			return -pte_eos;

		write(&pos, &header, sizeof(header));
		write(&pos, event->record.mmap, sizeof(*event->record.mmap));
		write(&pos, event->record.mmap->filename, slen);
		clear(&pos, gap);
	}
		break;

	case PERF_RECORD_LOST:
		size += sizeof(*event->record.lost);
		if (UINT16_MAX < size)
			return -pte_bad_packet;

		header.size = (uint16_t) size;
		if (end < pos + header.size)
			return -pte_eos;

		write(&pos, &header, sizeof(header));
		write(&pos, event->record.lost, sizeof(*event->record.lost));
		break;

	case PERF_RECORD_COMM: {
		size_t slen, gap;

		slen = strlen(event->record.comm->comm) + 1;
		gap = ((slen + 7) & ~7) - slen;

		size += sizeof(*event->record.comm) + slen + gap;
		if (UINT16_MAX < size)
			return -pte_bad_packet;

		header.size = (uint16_t) size;
		if (end < pos + header.size)
			return -pte_eos;

		write(&pos, &header, sizeof(header));
		write(&pos, event->record.comm, sizeof(*event->record.comm));
		write(&pos, event->record.comm->comm, slen);
		clear(&pos, gap);
	}
		break;

	case PERF_RECORD_EXIT:
		size += sizeof(*event->record.exit);
		if (UINT16_MAX < size)
			return -pte_bad_packet;

		header.size = (uint16_t) size;
		if (end < pos + header.size)
			return -pte_eos;

		write(&pos, &header, sizeof(header));
		write(&pos, event->record.exit, sizeof(*event->record.exit));
		break;

	case PERF_RECORD_THROTTLE:
	case PERF_RECORD_UNTHROTTLE:
		size += sizeof(*event->record.throttle);
		if (UINT16_MAX < size)
			return -pte_bad_packet;

		header.size = (uint16_t) size;
		if (end < pos + header.size)
			return -pte_eos;

		write(&pos, &header, sizeof(header));
		write(&pos, event->record.throttle,
		      sizeof(*event->record.throttle));
		break;

	case PERF_RECORD_FORK:
		size += sizeof(*event->record.fork);
		if (UINT16_MAX < size)
			return -pte_bad_packet;

		header.size = (uint16_t) size;
		if (end < pos + header.size)
			return -pte_eos;

		write(&pos, &header, sizeof(header));
		write(&pos, event->record.fork, sizeof(*event->record.fork));
		break;

	case PERF_RECORD_MMAP2: {
		size_t slen, gap;

		slen = strlen(event->record.mmap2->filename) + 1;
		gap = ((slen + 7) & ~7) - slen;

		size += sizeof(*event->record.mmap2) + slen + gap;
		if (UINT16_MAX < size)
			return -pte_bad_packet;

		header.size = (uint16_t) size;
		if (end < pos + header.size)
			return -pte_eos;

		write(&pos, &header, sizeof(header));
		write(&pos, event->record.mmap2, sizeof(*event->record.mmap2));
		write(&pos, event->record.mmap2->filename, slen);
		clear(&pos, gap);
	}
		break;

	case PERF_RECORD_AUX:
		size += sizeof(*event->record.aux);
		if (UINT16_MAX < size)
			return -pte_bad_packet;

		header.size = (uint16_t) size;
		if (end < pos + header.size)
			return -pte_eos;

		write(&pos, &header, sizeof(header));
		write(&pos, event->record.aux, sizeof(*event->record.aux));
		break;

	case PERF_RECORD_ITRACE_START:
		size += sizeof(*event->record.itrace_start);
		if (UINT16_MAX < size)
			return -pte_bad_packet;

		header.size = (uint16_t) size;
		if (end < pos + header.size)
			return -pte_eos;

		write(&pos, &header, sizeof(header));
		write(&pos, event->record.itrace_start,
		      sizeof(*event->record.itrace_start));
		break;

	case PERF_RECORD_LOST_SAMPLES:
		size += sizeof(*event->record.lost_samples);
		if (UINT16_MAX < size)
			return -pte_bad_packet;

		header.size = (uint16_t) size;
		if (end < pos + header.size)
			return -pte_eos;

		write(&pos, &header, sizeof(header));
		write(&pos, event->record.lost_samples,
		      sizeof(*event->record.lost_samples));
		break;

	case PERF_RECORD_SWITCH:
		header.size = (uint16_t) size;
		if (end < pos + header.size)
			return -pte_eos;

		write(&pos, &header, sizeof(header));
		break;

	case PERF_RECORD_SWITCH_CPU_WIDE:
		size += sizeof(*event->record.switch_cpu_wide);
		if (UINT16_MAX < size)
			return -pte_bad_packet;

		header.size = (uint16_t) size;
		if (end < pos + header.size)
			return -pte_eos;

		write(&pos, &header, sizeof(header));
		write(&pos, event->record.switch_cpu_wide,
		      sizeof(*event->record.switch_cpu_wide));
		break;
	}

	errcode = write_samples(&pos, event, config);
	if (errcode < 0)
		return errcode;

	return (int) (pos - begin);
}
