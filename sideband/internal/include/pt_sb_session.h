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

#ifndef PT_SB_SESSION_H
#define PT_SB_SESSION_H

#include "libipt-sb.h"

struct pt_image_section_cache;
struct pt_image;
struct pt_sb_context;
struct pt_sb_decoder;


struct pt_sb_session {
	/* The image section cache to use for new image sections.
	 *
	 * This allows sharing image sections across contexts.
	 */
	struct pt_image_section_cache *iscache;

	/* A linear list of contexts in no particular order. */
	struct pt_sb_context *contexts;

	/* The kernel memory image.
	 *
	 * Just like process images, the kernel image may change over time.  It
	 * is used to populate new process images.
	 *
	 * This assumes that the full kernel is mapped into every process.
	 */
	struct pt_image *kernel;

	/* A list of sideband decoders ordered by their @tsc (ascending). */
	struct pt_sb_decoder *decoders;

	/* A list of newly added sideband decoders in no particular order.
	 *
	 * Use pt_sb_init_decoders() to fetch the first record and move them to
	 * @decoders.
	 */
	struct pt_sb_decoder *waiting;

	/* A list of retired sideband decoders in no particular order.
	 *
	 * They ran out of trace but might still have a postponed effect
	 * pending.  We present events to them until @apply() returns -pte_eos.
	 */
	struct pt_sb_decoder *retired;

	/* A list of removed sideband decoders in no particular order.
	 *
	 * They wait for their destruction when the session is destroyed.
	 */
	struct pt_sb_decoder *removed;

	/* An optional callback function to be called on sideband decode errors
	 * and warnings.
	 */
	pt_sb_error_notifier_t *notify_error;

	/* The private data for the error notifier. */
	void *priv_error;

	/* An optional callback function to be called on context switches. */
	pt_sb_ctx_switch_notifier_t *notify_switch_to;

	/* The private data for the context switch notifier. */
	void *priv_switch_to;
};


extern int pt_sb_error(const struct pt_sb_session *session, int errcode,
		       const char *filename, uint64_t offset);

#endif /* PT_SB_SESSION_H */
