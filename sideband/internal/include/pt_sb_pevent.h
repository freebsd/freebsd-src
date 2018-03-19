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

#ifndef PT_SB_PEVENT_H
#define PT_SB_PEVENT_H

#include "pevent.h"


/* The estimated code location. */
enum pt_sb_pevent_loc {
	/* We do not know where we are. */
	ploc_unknown,

	/* We are in kernel space. */
	ploc_in_kernel,

	/* We are in user space. */
	ploc_in_user,

	/* We are likely in kernel space. */
	ploc_likely_in_kernel,

	/* We are likely in user space. */
	ploc_likely_in_user
};

/* A Linux perf event decoder's private data. */
struct pt_sb_pevent_priv {
	/* The sideband filename for printing.
	 *
	 * This is a copy of the filename provided by the user when allocating
	 * the sideband decoder.
	 */
	char *filename;

	/* The optional system root directory.
	 *
	 * If not NULL, this is prepended to every filename referenced in perf
	 * event sideband records.
	 *
	 * This is a copy of the sysroot provided by the user when allocating
	 * the sideband decoder.
	 */
	char *sysroot;

	/* The optional 64-bit vdso.
	 *
	 * If not NULL, this is used for [vdso] mmaps in 64-bit processes.
	 *
	 * This is a copy of the vdso filename provided by the user when
	 * allocating the sideband decoder.
	 */
	char *vdso_x64;

	/* The optional x32 vdso.
	 *
	 * If not NULL, this is used for [vdso] mmaps in x32 processes.
	 *
	 * This is a copy of the vdso filename provided by the user when
	 * allocating the sideband decoder.
	 */
	char *vdso_x32;

	/* The optional 32-bit vdso.
	 *
	 * If not NULL, this is used for [vdso] mmaps in 32-bit processes.
	 *
	 * This is a copy of the vdso filename provided by the user when
	 * allocating the sideband decoder.
	 */
	char *vdso_ia32;

	/* The begin and end of the sideband data in memory. */
	uint8_t *begin, *end;

	/* The position of the current and the next record in the sideband
	 * buffer.
	 *
	 * The current position is the position of @event or NULL.
	 * the next position is the position from which to fetch.
	 */
	const uint8_t *current, *next;

	/* The libpevent configuration. */
	struct pev_config pev;

	/* The current perf event record. */
	struct pev_event event;

	/* The current process context.
	 *
	 * This is NULL if there is no current context.
	 * Otherwise, holds a reference to @context (put after use).
	 */
	struct pt_sb_context *context;

	/* The next process context.
	 *
	 * This is NULL if we're not waiting to switch contexts.
	 * Otherwise, holds a reference to @context (put after use).
	 */
	struct pt_sb_context *next_context;

	/* The start address of the kernel.
	 *
	 * This is used to distinguish kernel from user addresses:
	 *
	 *   kernel >= @kernel_start
	 *   user   <  @kernel_start
	 *
	 * This is only required when tracing ring-0.
	 */
	uint64_t kernel_start;

	/* An offset to be subtracted from every sideband record timestamp.
	 *
	 * This applies sideband records a little bit earlier to compensate for
	 * too coarse timing.
	 */
	uint64_t tsc_offset;

	/* The current code location estimated from previous events. */
	enum pt_sb_pevent_loc location;
};

extern int pt_sb_pevent_init(struct pt_sb_pevent_priv *priv,
			     const struct pt_sb_pevent_config *config);

#endif /* PT_SB_PEVENT_H */
