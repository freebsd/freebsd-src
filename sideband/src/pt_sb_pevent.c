/*
 * Copyright (c) 2017-2018, Intel Corporation
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

#include "libipt-sb.h"

#include "intel-pt.h"


#ifndef FEATURE_PEVENT

int pt_sb_alloc_pevent_decoder(struct pt_sb_session *session,
			       const struct pt_sb_pevent_config *config)
{
	(void) session;
	(void) config;

	return -pte_not_supported;
}

#else /* FEATURE_PEVENT */

#include "pt_sb_pevent.h"
#include "pt_sb_session.h"
#include "pt_sb_context.h"
#include "pt_sb_file.h"
#include "pt_compiler.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER) && (_MSC_VER < 1900)
#  define snprintf _snprintf_c
#endif


#ifndef FEATURE_ELF

static int elf_get_abi(FILE *file)
{
	if (!file)
		return -pte_internal;

	return pt_sb_abi_unknown;
}

#else /* FEATURE_ELF */

#include <elf.h>


static int elf_get_abi(FILE *file)
{
	uint8_t e_ident[EI_NIDENT];
	size_t count;
	int status;

	if (!file)
		return -pte_internal;

	status = fseek(file, 0, SEEK_SET);
	if (status < 0)
		return pt_sb_abi_unknown;

	count = fread(e_ident, sizeof(e_ident), 1, file);
	if (count != 1)
		return pt_sb_abi_unknown;

	status = memcmp(e_ident, ELFMAG, SELFMAG);
	if (status != 0)
		return pt_sb_abi_unknown;

	if (e_ident[EI_VERSION] != EV_CURRENT)
		return pt_sb_abi_unknown;

	switch (e_ident[EI_CLASS]) {
	default:
		break;

	case ELFCLASS64:
		return pt_sb_abi_x64;

	case ELFCLASS32: {
		Elf32_Ehdr ehdr;

		status = fseek(file, 0, SEEK_SET);
		if (status < 0)
			break;

		count = fread(&ehdr, sizeof(ehdr), 1, file);
		if (count != 1)
			break;

		switch (ehdr.e_machine) {
		default:
			break;

		case EM_386:
			return pt_sb_abi_ia32;

		case EM_X86_64:
			return pt_sb_abi_x32;
		}
	}
		break;
	}

	return pt_sb_abi_unknown;
}

#endif /* FEATURE_ELF */

static int pt_sb_pevent_error(const struct pt_sb_session *session, int errcode,
			      const struct pt_sb_pevent_priv *priv)
{
	const char *filename;
	uint64_t offset;

	filename = NULL;
	offset = 0ull;

	if (priv) {
		const uint8_t *pos;

		pos = priv->current;
		if (!pos)
			pos = priv->next;

		filename = priv->filename;
		offset = (uint64_t) (pos - priv->begin);
	}

	return pt_sb_error(session, errcode, filename, offset);
}

static int pt_sb_pevent_track_abi(struct pt_sb_context *context,
				  const char *filename)
{
	FILE *file;
	int abi;

	if (!context || !filename)
		return -pte_internal;

	if (context->abi)
		return 0;

	file = fopen(filename, "rb");
	if (!file)
		return 0;

	abi = elf_get_abi(file);

	fclose(file);

	if (abi < 0)
		return abi;

	context->abi = (enum pt_sb_abi) abi;

	return 0;
}

static int pt_sb_pevent_find_vdso(const char **pvdso,
				  const struct pt_sb_pevent_priv *priv,
				  const struct pt_sb_context *context)
{
	const char *vdso;

	if (!pvdso || !priv || !context)
		return -pte_internal;

	vdso = NULL;

	switch (context->abi) {
	case pt_sb_abi_unknown:
		break;

	case pt_sb_abi_x64:
		vdso = priv->vdso_x64;
		break;

	case pt_sb_abi_x32:
		vdso = priv->vdso_x32;
		break;

	case pt_sb_abi_ia32:
		vdso = priv->vdso_ia32;
		break;
	}

	if (!vdso)
		return -pte_bad_config;

	*pvdso = vdso;

	return 0;
}

static void pt_sb_pevent_dtor(void *priv_arg)
{
	struct pt_sb_pevent_priv *priv;
	struct pt_sb_context *context;

	priv = (struct pt_sb_pevent_priv *) priv_arg;
	if (!priv)
		return;

	context = priv->next_context;
	if (context)
		pt_sb_ctx_put(context);

	context = priv->context;
	if (context)
		pt_sb_ctx_put(context);

	free(priv->filename);
	free(priv->sysroot);
	free(priv->vdso_x64);
	free(priv->vdso_x32);
	free(priv->vdso_ia32);
	free(priv->begin);
	free(priv);
}

static int pt_sb_pevent_init_path(char **dst, const char *src)
{
	size_t len;
	char *copy;

	if (!dst)
		return -pte_internal;

	if (!src) {
		*dst = NULL;
		return 0;
	}

	len = strnlen(src, FILENAME_MAX);
	if (len == FILENAME_MAX)
		return -pte_invalid;

	len += 1;
	copy = malloc(len);
	if (!copy)
		return -pte_nomem;

	memcpy(copy, src, len);

	*dst = copy;

	return 0;
}

int pt_sb_pevent_init(struct pt_sb_pevent_priv *priv,
		      const struct pt_sb_pevent_config *config)
{
	const char *filename;
	size_t size;
	void *buffer;
	int errcode;

	if (!priv || !config)
		return -pte_internal;

	/* This is the first version - we need all the fields. */
	if (config->size < sizeof(*config))
		return -pte_invalid;

	filename = config->filename;
	if (!filename)
		return -pte_invalid;

	buffer = NULL;
	size = 0;
	errcode = pt_sb_file_load(&buffer, &size, filename,
				  config->begin, config->end);
	if (errcode < 0)
		return errcode;

	memset(priv, 0, sizeof(*priv));
	priv->begin = (uint8_t *) buffer;
	priv->end = (uint8_t *) buffer + size;
	priv->next = (uint8_t *) buffer;

	errcode = pt_sb_pevent_init_path(&priv->filename, filename);
	if (errcode < 0) {
		pt_sb_pevent_dtor(priv);
		return errcode;
	}

	errcode = pt_sb_pevent_init_path(&priv->sysroot, config->sysroot);
	if (errcode < 0) {
		pt_sb_pevent_dtor(priv);
		return errcode;
	}

	errcode = pt_sb_pevent_init_path(&priv->vdso_x64, config->vdso_x64);
	if (errcode < 0) {
		pt_sb_pevent_dtor(priv);
		return errcode;
	}

	errcode = pt_sb_pevent_init_path(&priv->vdso_x32, config->vdso_x32);
	if (errcode < 0) {
		pt_sb_pevent_dtor(priv);
		return errcode;
	}

	errcode = pt_sb_pevent_init_path(&priv->vdso_ia32, config->vdso_ia32);
	if (errcode < 0) {
		pt_sb_pevent_dtor(priv);
		return errcode;
	}

	pev_config_init(&priv->pev);
	priv->pev.sample_type = config->sample_type;
	priv->pev.time_shift = config->time_shift;
	priv->pev.time_mult = config->time_mult;
	priv->pev.time_zero = config->time_zero;

	priv->kernel_start = config->kernel_start;
	priv->tsc_offset = config->tsc_offset;
	priv->location = ploc_unknown;

	return 0;
}

static int pt_sb_pevent_fetch(uint64_t *ptsc, struct pt_sb_pevent_priv *priv)
{
	struct pev_event *event;
	const uint8_t *pos;
	uint64_t tsc, offset;
	int size;

	if (!ptsc || !priv)
		return -pte_internal;

	pos = priv->next;
	event = &priv->event;

	/* Consume the current record early so we get the offset right when
	 * diagnosing fetch errors.
	 */
	priv->current = pos;

	size = pev_read(event, pos, priv->end, &priv->pev);
	if (size < 0)
		return size;

	priv->next = pos + size;

	/* If we don't have a time sample, set @ptsc to zero to process the
	 * record immediately.
	 */
	if (!event->sample.time) {
		*ptsc = 0ull;
		return 0;
	}

	/* Subtract a pre-defined offset to cause sideband events from this
	 * channel to be applied a little earlier.
	 *
	 * We don't want @tsc to wrap around when subtracting @offset, though.
	 * This would suddenly push the event very far out and essentially block
	 * this sideband channel.
	 *
	 * On the other hand, we want to allow 'negative' offsets.  And for
	 * those, we want to avoid wrapping around in the other direction.
	 */
	offset = priv->tsc_offset;
	tsc = event->sample.tsc;
	if (offset <= tsc)
		tsc -= offset;
	else {
		if (0ll <= (int64_t) offset)
			tsc = 0ull;
		else {
			if (tsc <= offset)
				tsc -= offset;
			else
				tsc = UINT64_MAX;
		}
	}

	/* We update the event record's timestamp, as well, so we will print the
	 * updated tsc and apply the event at the right time.
	 *
	 * Note that we only update our copy in @priv, not the sideband stream.
	 */
	event->sample.tsc = tsc;
	*ptsc = tsc;

	return 0;
}

static int pt_sb_pevent_print_event(const struct pev_event *event,
				    FILE *stream, uint32_t flags)
{
	if (!event)
		return -pte_internal;

	switch (event->type) {
	default:
		if (flags & ptsbp_compact)
			fprintf(stream, "UNKNOWN (%x, %x)", event->type,
				event->misc);

		if (flags & ptsbp_verbose) {
			fprintf(stream, "UNKNOWN");
			fprintf(stream, "\n  type: %x", event->type);
			fprintf(stream, "\n  misc: %x", event->misc);
		}

		break;

	case PERF_RECORD_MMAP: {
		const struct pev_record_mmap *mmap;

		mmap = event->record.mmap;
		if (!mmap)
			return -pte_bad_packet;

		if (flags & ptsbp_compact)
			fprintf(stream, "PERF_RECORD_MMAP  %x/%x, %" PRIx64
				", %" PRIx64 ", %" PRIx64 ", %s",
				mmap->pid, mmap->tid, mmap->addr, mmap->len,
				mmap->pgoff, mmap->filename);

		if (flags & ptsbp_verbose) {
			fprintf(stream, "PERF_RECORD_MMAP");
			fprintf(stream, "\n  pid: %x", mmap->pid);
			fprintf(stream, "\n  tid: %x", mmap->tid);
			fprintf(stream, "\n  addr: %" PRIx64, mmap->addr);
			fprintf(stream, "\n  len: %" PRIx64, mmap->len);
			fprintf(stream, "\n  pgoff: %" PRIx64, mmap->pgoff);
			fprintf(stream, "\n  filename: %s", mmap->filename);
		}
	}
		break;

	case PERF_RECORD_LOST: {
		const struct pev_record_lost *lost;

		lost = event->record.lost;
		if (!lost)
			return -pte_bad_packet;

		if (flags & ptsbp_compact)
			fprintf(stream, "PERF_RECORD_LOST  %" PRIx64 ", %"
				PRIx64, lost->id, lost->lost);

		if (flags & ptsbp_verbose) {
			fprintf(stream, "PERF_RECORD_LOST");
			fprintf(stream, "\n  id: %" PRIx64, lost->id);
			fprintf(stream, "\n  lost: %" PRIx64, lost->lost);
		}
	}
		break;

	case PERF_RECORD_COMM: {
		const struct pev_record_comm *comm;
		const char *sfx;

		comm = event->record.comm;
		if (!comm)
			return -pte_bad_packet;

		sfx = event->misc & PERF_RECORD_MISC_COMM_EXEC ? ".EXEC" : "";

		if (flags & ptsbp_compact)
			fprintf(stream, "PERF_RECORD_COMM%s  %x/%x, %s", sfx,
				comm->pid, comm->tid, comm->comm);

		if (flags & ptsbp_verbose) {
			fprintf(stream, "PERF_RECORD_COMM%s", sfx);
			fprintf(stream, "\n  pid: %x", comm->pid);
			fprintf(stream, "\n  tid: %x", comm->tid);
			fprintf(stream, "\n  comm: %s", comm->comm);
		}
	}
		break;

	case PERF_RECORD_EXIT: {
		const struct pev_record_exit *exit;

		exit = event->record.exit;
		if (!exit)
			return -pte_bad_packet;

		if (flags & ptsbp_compact)
			fprintf(stream, "PERF_RECORD_EXIT  %x/%x, %x/%x, %"
				PRIx64, exit->pid, exit->tid, exit->ppid,
				exit->ptid, exit->time);

		if (flags & ptsbp_verbose) {
			fprintf(stream, "PERF_RECORD_EXIT");
			fprintf(stream, "\n  pid: %x", exit->pid);
			fprintf(stream, "\n  ppid: %x", exit->ppid);
			fprintf(stream, "\n  tid: %x", exit->tid);
			fprintf(stream, "\n  ptid: %x", exit->ptid);
			fprintf(stream, "\n  time: %" PRIx64, exit->time);
		}
	}
		break;

	case PERF_RECORD_THROTTLE: {
		const struct pev_record_throttle *throttle;

		throttle = event->record.throttle;
		if (!throttle)
			return -pte_bad_packet;

		if (flags & ptsbp_compact)
			fprintf(stream, "PERF_RECORD_THROTTLE  %" PRIx64 ", %"
				PRIx64 ", %" PRIx64, throttle->time,
				throttle->id, throttle->stream_id);

		if (flags & ptsbp_verbose) {
			fprintf(stream, "PERF_RECORD_THROTTLE");
			fprintf(stream, "\n  time: %" PRIx64, throttle->time);
			fprintf(stream, "\n  id: %" PRIx64, throttle->id);
			fprintf(stream, "\n  stream_id: %" PRIx64,
				throttle->stream_id);
		}
	}
		break;

	case PERF_RECORD_UNTHROTTLE: {
		const struct pev_record_throttle *throttle;

		throttle = event->record.throttle;
		if (!throttle)
			return -pte_bad_packet;

		if (flags & ptsbp_compact)
			fprintf(stream, "PERF_RECORD_UNTHROTTLE  %" PRIx64
				", %" PRIx64 ", %" PRIx64, throttle->time,
				throttle->id, throttle->stream_id);

		if (flags & ptsbp_verbose) {
			fprintf(stream, "PERF_RECORD_UNTHROTTLE");
			fprintf(stream, "\n  time: %" PRIx64, throttle->time);
			fprintf(stream, "\n  id: %" PRIx64, throttle->id);
			fprintf(stream, "\n  stream_id: %" PRIx64,
				throttle->stream_id);
		}
	}
		break;

	case PERF_RECORD_FORK: {
		const struct pev_record_fork *fork;

		fork = event->record.fork;
		if (!fork)
			return -pte_bad_packet;

		if (flags & ptsbp_compact)
			fprintf(stream, "PERF_RECORD_FORK  %x/%x, %x/%x, %"
				PRIx64, fork->pid, fork->tid, fork->ppid,
				fork->ptid, fork->time);

		if (flags & ptsbp_verbose) {
			fprintf(stream, "PERF_RECORD_FORK");
			fprintf(stream, "\n  pid: %x", fork->pid);
			fprintf(stream, "\n  ppid: %x", fork->ppid);
			fprintf(stream, "\n  tid: %x", fork->tid);
			fprintf(stream, "\n  ptid: %x", fork->ptid);
			fprintf(stream, "\n  time: %" PRIx64, fork->time);
		}
	}
		break;

	case PERF_RECORD_MMAP2: {
		const struct pev_record_mmap2 *mmap2;

		mmap2 = event->record.mmap2;
		if (!mmap2)
			return -pte_bad_packet;

		if (flags & ptsbp_compact)
			fprintf(stream, "PERF_RECORD_MMAP2  %x/%x, %" PRIx64
				", %" PRIx64 ", %" PRIx64 ", %x, %x, %" PRIx64
				", %" PRIx64 ", %x, %x, %s", mmap2->pid,
				mmap2->tid, mmap2->addr, mmap2->len,
				mmap2->pgoff, mmap2->maj, mmap2->min,
				mmap2->ino, mmap2->ino_generation, mmap2->prot,
				mmap2->flags, mmap2->filename);

		if (flags & ptsbp_verbose) {
			fprintf(stream, "PERF_RECORD_MMAP2");
			fprintf(stream, "\n  pid: %x", mmap2->pid);
			fprintf(stream, "\n  tid: %x", mmap2->tid);
			fprintf(stream, "\n  addr: %" PRIx64, mmap2->addr);
			fprintf(stream, "\n  len: %" PRIx64, mmap2->len);
			fprintf(stream, "\n  pgoff: %" PRIx64, mmap2->pgoff);
			fprintf(stream, "\n  maj: %x", mmap2->maj);
			fprintf(stream, "\n  min: %x", mmap2->min);
			fprintf(stream, "\n  ino: %" PRIx64, mmap2->ino);
			fprintf(stream, "\n  ino_generation: %" PRIx64,
				mmap2->ino_generation);
			fprintf(stream, "\n  prot: %x", mmap2->prot);
			fprintf(stream, "\n  flags: %x", mmap2->flags);
			fprintf(stream, "\n  filename: %s", mmap2->filename);
		}
	}
		break;

	case PERF_RECORD_AUX: {
		const struct pev_record_aux *aux;
		const char *sfx;

		aux = event->record.aux;
		if (!aux)
			return -pte_bad_packet;

		sfx = aux->flags & PERF_AUX_FLAG_TRUNCATED ? ".TRUNCATED" : "";

		if (flags & ptsbp_compact)
			fprintf(stream, "PERF_RECORD_AUX%s  %" PRIx64 ", %"
				PRIx64 ", %" PRIx64, sfx, aux->aux_offset,
				aux->aux_size, aux->flags);

		if (flags & ptsbp_verbose) {
			fprintf(stream, "PERF_RECORD_AUX%s", sfx);
			fprintf(stream, "\n  aux offset: %" PRIx64,
				aux->aux_offset);
			fprintf(stream, "\n  aux size: %" PRIx64,
				aux->aux_size);
			fprintf(stream, "\n  flags: %" PRIx64, aux->flags);
		}
	}
		break;

	case PERF_RECORD_ITRACE_START: {
		const struct pev_record_itrace_start *itrace_start;

		itrace_start = event->record.itrace_start;
		if (!itrace_start)
			return -pte_bad_packet;

		if (flags & ptsbp_compact)
			fprintf(stream, "PERF_RECORD_ITRACE_START  %x/%x",
				itrace_start->pid, itrace_start->tid);

		if (flags & ptsbp_verbose) {
			fprintf(stream, "PERF_RECORD_ITRACE_START");
			fprintf(stream, "\n  pid: %x", itrace_start->pid);
			fprintf(stream, "\n  tid: %x", itrace_start->tid);
		}
	}
		break;

	case PERF_RECORD_LOST_SAMPLES: {
		const struct pev_record_lost_samples *lost_samples;

		lost_samples = event->record.lost_samples;
		if (!lost_samples)
			return -pte_bad_packet;

		if (flags & ptsbp_compact)
			fprintf(stream, "PERF_RECORD_LOST_SAMPLES  %" PRIx64,
				lost_samples->lost);

		if (flags & ptsbp_verbose) {
			fprintf(stream, "PERF_RECORD_LOST_SAMPLES");
			fprintf(stream, "\n  lost: %" PRIx64,
				lost_samples->lost);
		}

	}
		break;

	case PERF_RECORD_SWITCH: {
		const char *sfx;

		sfx = event->misc & PERF_RECORD_MISC_SWITCH_OUT ? "OUT" : "IN";

		if (flags & (ptsbp_compact | ptsbp_verbose))
			fprintf(stream, "PERF_RECORD_SWITCH.%s", sfx);
	}
		break;

	case PERF_RECORD_SWITCH_CPU_WIDE: {
		const struct pev_record_switch_cpu_wide *switch_cpu_wide;
		const char *sfx, *pfx;

		if (event->misc & PERF_RECORD_MISC_SWITCH_OUT) {
			sfx = "OUT";
			pfx = "next";
		} else {
			sfx = "IN";
			pfx = "prev";
		}

		switch_cpu_wide = event->record.switch_cpu_wide;
		if (!switch_cpu_wide)
			return -pte_bad_packet;

		if (flags & ptsbp_compact)
			fprintf(stream, "PERF_RECORD_SWITCH_CPU_WIDE.%s  %x/%x",
				sfx, switch_cpu_wide->next_prev_pid,
				switch_cpu_wide->next_prev_tid);

		if (flags & ptsbp_verbose) {
			fprintf(stream, "PERF_RECORD_SWITCH_CPU_WIDE.%s", sfx);
			fprintf(stream, "\n  %s pid: %x", pfx,
				switch_cpu_wide->next_prev_pid);
			fprintf(stream, "\n  %s tid: %x", pfx,
				switch_cpu_wide->next_prev_tid);
		}
	}
		break;
	}

	return 0;
}

static int pt_sb_pevent_print_samples_compact(const struct pev_event *event,
					      FILE *stream)
{
	if (!event)
		return -pte_internal;

	fprintf(stream, "  {");

	if (event->sample.pid && event->sample.tid)
		fprintf(stream, " %x/%x", *event->sample.pid,
			*event->sample.tid);

	if (event->sample.time)
		fprintf(stream, " %" PRIx64, *event->sample.time);

	if (event->sample.id)
		fprintf(stream, " %" PRIx64, *event->sample.id);

	if (event->sample.cpu)
		fprintf(stream, " cpu-%x", *event->sample.cpu);

	if (event->sample.stream_id)
		fprintf(stream, " %" PRIx64, *event->sample.stream_id);

	if (event->sample.identifier)
		fprintf(stream, " %" PRIx64, *event->sample.identifier);

	fprintf(stream, " }");

	return 0;
}

static int pt_sb_pevent_print_samples_verbose(const struct pev_event *event,
					      FILE *stream)
{
	if (!event)
		return -pte_internal;

	if (event->sample.pid && event->sample.tid) {
		fprintf(stream, "\n  pid: %x", *event->sample.pid);
		fprintf(stream, "\n  tid: %x", *event->sample.tid);
	}

	if (event->sample.time)
		fprintf(stream, "\n  time: %" PRIx64, *event->sample.time);

	if (event->sample.id)
		fprintf(stream, "\n  id: %" PRIx64, *event->sample.id);

	if (event->sample.cpu)
		fprintf(stream, "\n  cpu: %x", *event->sample.cpu);

	if (event->sample.stream_id)
		fprintf(stream, "\n  stream id: %" PRIx64,
			*event->sample.stream_id);

	if (event->sample.identifier)
		fprintf(stream, "\n  identifier: %" PRIx64,
			*event->sample.identifier);

	return 0;
}

static int pt_sb_pevent_print_samples(const struct pev_event *event,
				      FILE *stream, uint32_t flags)
{
	int errcode;

	if (flags & ptsbp_compact) {
		errcode = pt_sb_pevent_print_samples_compact(event, stream);
		if (errcode < 0)
			return errcode;
	}

	if (flags & ptsbp_verbose) {
		errcode = pt_sb_pevent_print_samples_verbose(event, stream);
		if (errcode < 0)
			return errcode;
	}

	return 0;
}

static int pt_sb_pevent_print(struct pt_sb_pevent_priv *priv, FILE *stream,
			      uint32_t flags)
{
	struct pev_event *event;
	int errcode;

	if (!priv)
		return -pte_internal;

	/* We should not be called before fetching the first record. */
	if (!priv->current)
		return -pte_internal;

	if (!priv->filename)
		return -pte_internal;

	event = &priv->event;

	/* Print filename and/or file offset before the actual record. */
	switch (flags & (ptsbp_filename | ptsbp_file_offset)) {
	case ptsbp_filename | ptsbp_file_offset:
		fprintf(stream, "%s:%016" PRIx64 "  ", priv->filename,
			(uint64_t) (priv->current - priv->begin));
		break;

	case ptsbp_filename:
		fprintf(stream, "%s  ", priv->filename);
		break;

	case ptsbp_file_offset:
		fprintf(stream, "%016" PRIx64 "  ",
			(uint64_t) (priv->current - priv->begin));
		break;
	}

	/* Print the timestamp if requested and available. */
	if ((flags & ptsbp_tsc) && event->sample.time)
		fprintf(stream, "%016" PRIx64 "  ", event->sample.tsc);

	/* Print the actual sideband record. */
	errcode = pt_sb_pevent_print_event(event, stream, flags);
	if (errcode < 0)
		return errcode;

	/* Print samples if configured. */
	if (priv->pev.sample_type) {
		errcode = pt_sb_pevent_print_samples(event, stream, flags);
		if (errcode < 0)
			return errcode;
	}

	if (flags)
		fprintf(stream, "\n");

	return 0;
}

static int pt_sb_pevent_switch_contexts(struct pt_sb_session *session,
					struct pt_image **image,
					struct pt_sb_pevent_priv *priv)
{
	struct pt_sb_context *prev, *next;
	int errcode;

	if (!priv || !image)
		return -pte_internal;

	prev = priv->context;
	next = priv->next_context;
	if (!next)
		return -pte_internal;

	errcode = pt_sb_ctx_switch_to(image, session, next);
	if (errcode < 0)
		return errcode;

	priv->next_context = NULL;
	priv->context = next;

	return prev ? pt_sb_ctx_put(prev) : 0;
}

static int pt_sb_pevent_cancel_context_switch(struct pt_sb_pevent_priv *priv)
{
	struct pt_sb_context *context;

	if (!priv)
		return -pte_internal;

	context = priv->next_context;
	if (!context)
		return 0;

	priv->next_context = NULL;

	return pt_sb_ctx_put(context);
}

static int pt_sb_pevent_prepare_context_switch(struct pt_sb_pevent_priv *priv,
					       struct pt_sb_context *context)
{
	int errcode;

	if (!priv || !context)
		return -pte_internal;

	/* There's nothing to do if this switch is already pending.
	 *
	 * This could be the result of applying a cpu-wide switch-out followed
	 * by a cpu-wide switch-in without a chance to actually apply the
	 * context switch in-between.
	 */
	if (priv->next_context == context)
		return 0;

	/* This context switch overwrites any previously pending switch.
	 *
	 * We may skip context switches due to imprecise timing or due to
	 * re-synchronization after an error.
	 */
	errcode = pt_sb_pevent_cancel_context_switch(priv);
	if (errcode < 0)
		return errcode;

	/* There's nothing to do if we're switching to the current context.
	 *
	 * This could be the result of switching between threads of the same
	 * process or of applying a cpu-wide switch-out followed by a cpu-wide
	 * switch-in.
	 */
	if (priv->context == context)
		return 0;

	errcode = pt_sb_ctx_get(context);
	if (errcode < 0)
		return errcode;

	priv->next_context = context;

	return 0;
}

static int pt_sb_pevent_prepare_switch_to_pid(struct pt_sb_session *session,
					      struct pt_sb_pevent_priv *priv,
					      uint32_t pid)
{
	struct pt_sb_context *context;
	int errcode;

	context = NULL;
	errcode = pt_sb_get_context_by_pid(&context, session, pid);
	if (errcode < 0)
		return errcode;

	return pt_sb_pevent_prepare_context_switch(priv, context);
}

static int pt_sb_pevent_remove_context_for_pid(struct pt_sb_session *session,
					       uint32_t pid)
{
	struct pt_sb_context *context;
	int errcode;

	context = NULL;
	errcode = pt_sb_find_context_by_pid(&context, session, pid);
	if (errcode < 0)
		return errcode;

	if (!context)
		return 0;

	return pt_sb_remove_context(session, context);
}

static int
pt_sb_pevent_itrace_start(struct pt_sb_session *session,
			  struct pt_image **image,
			  struct pt_sb_pevent_priv *priv,
			  const struct pev_record_itrace_start *record)
{
	int errcode;

	if (!image || !record)
		return -pte_internal;

	errcode = pt_sb_pevent_prepare_switch_to_pid(session, priv,
						     record->pid);
	if (errcode < 0)
		return errcode;

	/* We may have already installed the starting context. */
	if (!priv->next_context)
		return 0;

	/* If we have not, let's not wait for a suitable event.
	 *
	 * We just started tracing so there's no reason to wait for a suitable
	 * location.
	 */
	return pt_sb_pevent_switch_contexts(session, image, priv);
}

static int pt_sb_pevent_fork(struct pt_sb_session *session,
			     const struct pev_record_fork *record)
{
	struct pt_sb_context *context, *parent;
	struct pt_image *image, *pimage;
	uint32_t ppid, pid;
	int errcode;

	if (!record)
		return -pte_internal;

	/* If this is just creating a new thread, there's nothing to do.
	 *
	 * We should already have a context for this process.  If we don't, it
	 * doesn't really help to create a new context with an empty process
	 * image at this point.
	 */
	ppid = record->ppid;
	pid = record->pid;
	if (ppid == pid)
		return 0;

	/* We're creating a new process plus the initial thread.
	 *
	 * That initial thread should get the same id as the process.
	 */
	if (pid != record->tid)
		return -pte_internal;

	/* Remove any existing context we might have for @pid.
	 *
	 * We're not removing process contexts when we get the exit event since
	 * that is sent while the process is still running inside the kernel.
	 */
	errcode = pt_sb_pevent_remove_context_for_pid(session, pid);
	if (errcode < 0)
		return errcode;

	/* Create a new context for this new process. */
	context = NULL;
	errcode = pt_sb_get_context_by_pid(&context, session, pid);
	if (errcode < 0)
		return errcode;

	/* Let's see if we also know about the parent process. */
	parent = NULL;
	errcode = pt_sb_find_context_by_pid(&parent, session, ppid);
	if (errcode < 0)
		return errcode;

	if (!parent)
		return 0;

	/* Both parent and child must have valid images. */
	pimage = pt_sb_ctx_image(parent);
	image = pt_sb_ctx_image(context);
	if (!pimage || !image)
		return -pte_internal;

	/* Initialize the child's image with its parent's. */
	return pt_image_copy(image, pimage);
}

static int pt_sb_pevent_exec(struct pt_sb_session *session,
			     struct pt_image **image,
			     struct pt_sb_pevent_priv *priv,
			     const struct pev_record_comm *record)
{
	struct pt_sb_context *context;
	uint32_t pid;
	int errcode;

	if (!record)
		return -pte_internal;

	pid = record->pid;

	/* Instead of replacing a context's image, we replace the context.
	 *
	 * This allows us to keep the old image around until we actually switch.
	 * We are likely using it at the moment.
	 */
	errcode = pt_sb_pevent_remove_context_for_pid(session, pid);
	if (errcode < 0)
		return errcode;

	/* This creates a new context and a new image.
	 *
	 * This new image will already be initialized with kernel sections but
	 * will otherwise be empty.  We will populate it later with MMAP records
	 * that follow this COMM.EXEC record.
	 */
	context = NULL;
	errcode = pt_sb_get_context_by_pid(&context, session, pid);
	if (errcode < 0)
		return errcode;

	/* If we're not maintaining a decoder, we're done. */
	if (!image)
		return 0;

	/* We replaced the previous context of @pid with @context.  Let's
	 * (prepare to) switch to the new @context.
	 *
	 * The actual context switch is postponed until we're in kernel context.
	 *
	 * It is quite likely that we are currently using the previous context
	 * we removed earlier in order to reach the location where we transition
	 * into the kernel.  In the trace, we have not yet exec'ed.
	 */
	return pt_sb_pevent_prepare_context_switch(priv, context);
}

static int pt_sb_pevent_switch(struct pt_sb_session *session,
			       struct pt_sb_pevent_priv *priv,
			       const uint32_t *pid)
{
	if (!pid)
		return -pte_bad_config;

	return pt_sb_pevent_prepare_switch_to_pid(session, priv, *pid);
}

static int
pt_sb_pevent_switch_cpu(struct pt_sb_session *session,
			struct pt_sb_pevent_priv *priv,
			const struct pev_record_switch_cpu_wide *record)
{
	if (!record)
		return -pte_internal;

	return pt_sb_pevent_prepare_switch_to_pid(session, priv,
						  record->next_prev_pid);
}

static int pt_sb_pevent_map(struct pt_sb_session *session,
			    const struct pt_sb_pevent_priv *priv, uint32_t pid,
			    const char *filename, uint64_t offset,
			    uint64_t size, uint64_t vaddr)
{
	struct pt_sb_context *context;
	const char *sysroot;
	char buffer[FILENAME_MAX];
	int errcode;

	if (!priv || !filename)
		return -pte_internal;

	/* Get the context for this process. */
	context = NULL;
	errcode = pt_sb_get_context_by_pid(&context, session, pid);
	if (errcode < 0)
		return errcode;

	/* The optional system root directoy. */
	sysroot = priv->sysroot;

	/* Some filenames do not represent actual files on disk.  We handle
	 * some of those and ignore the rest.
	 */
	if (filename[0] == '[') {
		/* The [vdso] file represents the vdso that is mapped into
		 * every process.
		 *
		 * We expect the user to provide all necessary vdso flavors.
		 */
		if (strcmp(filename, "[vdso]") == 0) {
			errcode = pt_sb_pevent_find_vdso(&filename, priv,
							 context);
			if (errcode != 0)
				return pt_sb_pevent_error(session, errcode,
							  priv);
		} else
			return pt_sb_pevent_error(session, ptse_section_lost,
						  priv);


	} else if (strcmp(filename, "//anon") == 0) {
		/* Those are anonymous mappings that are, for example, used by
		 * JIT compilers to generate code in that is later executed.
		 *
		 * There is no general support for this in perf and JIT enabling
		 * is work-in-progress.
		 *
		 * We will likely fail with -pte_nomap later on.
		 */
		return pt_sb_pevent_error(session, ptse_section_lost, priv);

	} else if (strstr(filename, " (deleted)")) {
		/* The file that was mapped as meanwhile been deleted.
		 *
		 * We will likely fail with -pte_nomap later on.
		 */
		return pt_sb_pevent_error(session, ptse_section_lost, priv);

	} else if (sysroot) {
		/* Prepend the sysroot to normal files. */
		errcode = snprintf(buffer, sizeof(buffer), "%s%s", sysroot,
				   filename);
		if (errcode < 0)
			return -pte_overflow;

		filename = buffer;
	}

	errcode = pt_sb_pevent_track_abi(context, filename);
	if (errcode < 0)
		return errcode;

	return pt_sb_ctx_mmap(session, context, filename, offset, size, vaddr);
}

static int pt_sb_pevent_mmap(struct pt_sb_session *session,
			     const struct pt_sb_pevent_priv *priv,
			     const struct pev_record_mmap *record)
{
	if (!record)
		return -pte_internal;

	return pt_sb_pevent_map(session, priv, record->pid, record->filename,
				record->pgoff, record->len, record->addr);
}

static int pt_sb_pevent_mmap2(struct pt_sb_session *session,
			      const struct pt_sb_pevent_priv *priv,
			      const struct pev_record_mmap2 *record)
{
	if (!record)
		return -pte_internal;

	return pt_sb_pevent_map(session, priv, record->pid, record->filename,
				record->pgoff, record->len, record->addr);
}

static int pt_sb_pevent_aux(const struct pt_sb_session *session,
			    const struct pt_sb_pevent_priv *priv,
			    const struct pev_record_aux *record)
{
	if (!record)
		return -pte_internal;

	if (record->flags & PERF_AUX_FLAG_TRUNCATED)
		return pt_sb_pevent_error(session, ptse_trace_lost, priv);

	return 0;
}

static int pt_sb_pevent_ignore_mmap(uint16_t misc)
{
	/* We rely on the kernel core file for ring-0 decode.
	 *
	 * Both kernel and kernel modules are modified during boot and insmod
	 * respectively.  We can't decode from the respective files on disk.
	 *
	 * Ignore kernel MMAP events so we don't overwrite useful data from
	 * kcore with useless data from binary files.
	 */
	switch (misc & PERF_RECORD_MISC_CPUMODE_MASK) {
	case PERF_RECORD_MISC_KERNEL:
		return 1;

	default:
		return 0;
	}
}

static int pt_sb_pevent_apply_event_record(struct pt_sb_session *session,
					   struct pt_image **image,
					   struct pt_sb_pevent_priv *priv,
					   const struct pev_event *event)
{
	if (!event)
		return -pte_internal;

	switch (event->type) {
	default:
		/* Ignore unknown events. */
		break;

	case PERF_RECORD_ITRACE_START:
		/* Ignore trace starts from secondary sideband channels. */
		if (!image)
			break;

		return pt_sb_pevent_itrace_start(session, image, priv,
						 event->record.itrace_start);

	case PERF_RECORD_FORK:
		return pt_sb_pevent_fork(session, event->record.fork);

	case PERF_RECORD_COMM:
		/* We're only interested in COMM.EXEC events. */
		if (!(event->misc & PERF_RECORD_MISC_COMM_EXEC))
			break;

		return pt_sb_pevent_exec(session, image, priv,
					 event->record.comm);

	case PERF_RECORD_SWITCH:
		/* Ignore context switches from secondary sideband channels. */
		if (!image)
			break;

		/* Ignore switch-out events.  We wait for the switch-in. */
		if (event->misc & PERF_RECORD_MISC_SWITCH_OUT)
			break;

		return pt_sb_pevent_switch(session, priv, event->sample.pid);

	case PERF_RECORD_SWITCH_CPU_WIDE:
		/* Ignore context switches from secondary sideband channels. */
		if (!image)
			break;

		/* For switch-in events, we use the pid sample, if available.
		 *
		 * For cpu-wide switch events, not sampling pid is acceptable
		 * since we get the pid in @prev_next_pid of a switch-out event.
		 *
		 * We will use a cpu-wide switch-in event, if possible, but we
		 * should be able to do without most of the time.
		 */
		if (!(event->misc & PERF_RECORD_MISC_SWITCH_OUT)) {
			if (!event->sample.pid)
				break;

			return pt_sb_pevent_switch(session, priv,
						   event->sample.pid);
		}

		return pt_sb_pevent_switch_cpu(session, priv,
					       event->record.switch_cpu_wide);

	case PERF_RECORD_MMAP:
		/* We intentionally ignore some MMAP records. */
		if (pt_sb_pevent_ignore_mmap(event->misc))
			break;

		return pt_sb_pevent_mmap(session, priv, event->record.mmap);

	case PERF_RECORD_MMAP2:
		/* We intentionally ignore some MMAP records. */
		if (pt_sb_pevent_ignore_mmap(event->misc))
			break;

		return pt_sb_pevent_mmap2(session, priv, event->record.mmap2);

	case PERF_RECORD_LOST:
		/* Warn about losses.
		 *
		 * We put the warning into the output.  It is quite likely that
		 * we will run into a decode error shortly after (or ran into it
		 * already); this warning may help explain it.
		 */
		return pt_sb_pevent_error(session, ptse_lost, priv);

	case PERF_RECORD_AUX:
		/* Ignore trace losses from secondary sideband channels. */
		if (!image)
			break;

		return pt_sb_pevent_aux(session, priv, event->record.aux);
	}

	return 0;
}

static int ploc_from_ip(enum pt_sb_pevent_loc *loc,
			const struct pt_sb_pevent_priv *priv, uint64_t ip)
{
	if (!loc || !priv)
		return -pte_internal;

	*loc = (ip < priv->kernel_start) ? ploc_in_user : ploc_in_kernel;

	return 0;
}

static int ploc_from_suppressed_ip(enum pt_sb_pevent_loc *loc,
				   enum pt_sb_pevent_loc from)
{
	if (!loc)
		return -pte_internal;

	switch (from) {
	default:
		*loc = ploc_unknown;
		break;

	case ploc_likely_in_kernel:
	case ploc_in_kernel:
		*loc = ploc_likely_in_user;
		break;

	case ploc_likely_in_user:
	case ploc_in_user:
		*loc = ploc_likely_in_kernel;
		break;
	}

	return 0;
}

static int ploc_from_event(enum pt_sb_pevent_loc *loc,
			   const struct pt_sb_pevent_priv *priv,
			   const struct pt_event *event)
{
	if (!loc || !priv || !event)
		return -pte_internal;

	switch (event->type) {
	default:
		break;

	case ptev_enabled:
		return ploc_from_ip(loc, priv, event->variant.enabled.ip);

	case ptev_disabled:
		if (!event->ip_suppressed)
			return ploc_from_ip(loc, priv,
					    event->variant.disabled.ip);

		return ploc_from_suppressed_ip(loc, priv->location);

	case ptev_async_disabled: {
		enum pt_sb_pevent_loc fromloc;
		int errcode;

		if (!event->ip_suppressed)
			return ploc_from_ip(loc, priv,
					    event->variant.async_disabled.ip);

		errcode = ploc_from_ip(&fromloc, priv,
				       event->variant.async_disabled.at);
		if (errcode < 0)
			return errcode;

		return ploc_from_suppressed_ip(loc, fromloc);
	}

	case ptev_async_branch:
		return ploc_from_ip(loc, priv, event->variant.async_branch.to);

	case ptev_async_paging:
		if (!event->ip_suppressed)
			return ploc_from_ip(loc, priv,
					    event->variant.async_paging.ip);

		fallthrough;
	case ptev_paging:
		*loc = ploc_likely_in_kernel;
		return 0;

	case ptev_overflow:
		if (!event->ip_suppressed)
			return ploc_from_ip(loc, priv,
					    event->variant.overflow.ip);

		break;

	case ptev_exec_mode:
		if (!event->ip_suppressed)
			return ploc_from_ip(loc, priv,
					    event->variant.exec_mode.ip);

		break;

	case ptev_tsx:
		if (!event->ip_suppressed)
			return ploc_from_ip(loc, priv,
					    event->variant.tsx.ip);

		break;

	case ptev_exstop:
		if (!event->ip_suppressed)
			return ploc_from_ip(loc, priv,
					    event->variant.exstop.ip);

		break;

	case ptev_mwait:
		if (!event->ip_suppressed)
			return ploc_from_ip(loc, priv,
					    event->variant.mwait.ip);

		break;

	case ptev_ptwrite:
		if (!event->ip_suppressed)
			return ploc_from_ip(loc, priv,
					    event->variant.ptwrite.ip);

		break;

	case ptev_tick:
		if (!event->ip_suppressed)
			return ploc_from_ip(loc, priv,
					    event->variant.tick.ip);

		break;
	}

	*loc = ploc_unknown;
	return 0;
}

static int pt_sb_pevent_apply(struct pt_sb_session *session,
			      struct pt_image **image,
			      const struct pt_event *event,
			      struct pt_sb_pevent_priv *priv)
{
	const struct pev_event *record;
	enum pt_sb_pevent_loc oldloc;
	int errcode;

	if (!priv || !event)
		return -pte_internal;

	/* If the current perf event record is due, apply it.
	 *
	 * We don't need to look at the actual event that provided the
	 * timestamp.  It suffices to know that time moved beyond the current
	 * perf event record.
	 *
	 * It is tempting to postpone applying the record until a suitable event
	 * but we need to ensure that records from different channels are
	 * applied in timestamp order.
	 *
	 * So we apply the record solely based on timestamps and postpone its
	 * effect until a suitable event.
	 *
	 * The last record in the trace won't be overridden and we have to take
	 * care to not apply it twice.  We need to keep it until we were able to
	 * place the last pending context switch.
	 */
	record = &priv->event;
	if ((priv->current != priv->next) &&
	    (!record->sample.time || (record->sample.tsc <= event->tsc)))
		return pt_sb_pevent_apply_event_record(session, image, priv,
						       record);

	/* We first apply all our sideband records one-by-one until we're in
	 * sync with the event.
	 *
	 * When we get here, we applied all previous sideband records.  Let's
	 * use the event to keep track of kernel vs user space and apply any
	 * postponed context switches.
	 *
	 * We preserve the previous location to detect returns from kernel to
	 * user space.
	 */
	oldloc = priv->location;
	errcode = ploc_from_event(&priv->location, priv, event);
	if (errcode < 0)
		return errcode;

	/* We postpone context switches until we reach a suitable location in
	 * the trace.  If we don't have a context switch pending, we're done.
	 */
	if (!priv->next_context) {
		/* Signal the end of the trace if the last event did not result
		 * in a postponed context switch or if that context switch had
		 * been applied at a previous event.
		 */
		if (priv->current == priv->next)
			return -pte_eos;

		return 0;
	}

	/* Apply a postponed context switch inside kernel mode.
	 *
	 * For our purposes it does not matter where exactly we are in the
	 * kernel.
	 *
	 * In order to catch the first event window right before a tracing
	 * enabled event after some time of tracing being disabled (or at the
	 * beginning of the trace), we also accept an unknown location.
	 */
	switch (oldloc) {
	case ploc_likely_in_kernel:
	case ploc_in_kernel:
	case ploc_unknown:
		return pt_sb_pevent_switch_contexts(session, image, priv);

	default:
		switch (priv->location) {
		case ploc_likely_in_kernel:
		case ploc_in_kernel:
		case ploc_unknown:
			return pt_sb_pevent_switch_contexts(session, image,
							    priv);

		default:
			break;
		}

		break;
	};

	return 0;
}

static int pt_sb_pevent_fetch_callback(struct pt_sb_session *session,
				       uint64_t *tsc, void *priv)
{
	int errcode;

	errcode = pt_sb_pevent_fetch(tsc, (struct pt_sb_pevent_priv *) priv);
	if ((errcode < 0) && (errcode != -pte_eos))
		pt_sb_pevent_error(session, errcode,
				   (struct pt_sb_pevent_priv *) priv);

	return errcode;
}

static int pt_sb_pevent_print_callback(struct pt_sb_session *session,
				       FILE *stream, uint32_t flags, void *priv)
{
	int errcode;

	errcode = pt_sb_pevent_print((struct pt_sb_pevent_priv *) priv, stream,
				     flags);
	if (errcode < 0)
		return pt_sb_pevent_error(session, errcode,
					  (struct pt_sb_pevent_priv *) priv);

	return 0;
}

static int pt_sb_pevent_apply_callback(struct pt_sb_session *session,
				       struct pt_image **image,
				       const struct pt_event *event, void *priv)
{
	int errcode;

	errcode = pt_sb_pevent_apply(session, image, event,
				     (struct pt_sb_pevent_priv *) priv);
	if ((errcode < 0) && (errcode != -pte_eos))
		return pt_sb_pevent_error(session, errcode,
					  (struct pt_sb_pevent_priv *) priv);

	return errcode;
}

int pt_sb_alloc_pevent_decoder(struct pt_sb_session *session,
			       const struct pt_sb_pevent_config *pev)
{
	struct pt_sb_decoder_config config;
	struct pt_sb_pevent_priv *priv;
	int errcode;

	if (!session || !pev)
		return -pte_invalid;

	priv = malloc(sizeof(*priv));
	if (!priv)
		return -pte_nomem;

	errcode = pt_sb_pevent_init(priv, pev);
	if (errcode < 0) {
		free(priv);
		return errcode;
	}

	memset(&config, 0, sizeof(config));
	config.fetch = pt_sb_pevent_fetch_callback;
	config.apply = pt_sb_pevent_apply_callback;
	config.print = pt_sb_pevent_print_callback;
	config.dtor = pt_sb_pevent_dtor;
	config.priv = priv;
	config.primary = pev->primary;

	errcode = pt_sb_alloc_decoder(session, &config);
	if (errcode < 0)
		free(priv);

	return errcode;
}

#endif /* FEATURE_PEVENT */
