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

#ifndef PT_SB_CONTEXT_H
#define PT_SB_CONTEXT_H

#include <stdint.h>

struct pt_image;


/* The ABI of the process. */
enum pt_sb_abi {
	pt_sb_abi_unknown,
	pt_sb_abi_x64,
	pt_sb_abi_x32,
	pt_sb_abi_ia32
};

struct pt_sb_context {
	/* The next context in a linear list of process contexts.
	 *
	 * I do not expect more than a few processes per tracing session.  And
	 * if we had many processes, we'd also have trace spanning many context
	 * switches and sideband decode won't be the bottleneck.
	 *
	 * This field is owned by the sideband tracing session to which this
	 * context belongs.
	 */
	struct pt_sb_context *next;

	/* The memory image of that process.
	 *
	 * You must hold a reference to this context as long as @image is used.
	 */
	struct pt_image *image;

	/* The ABI of the process.
	 *
	 * This may be relevant for some but not all sideband formats.
	 *
	 * This field is collectively owned by all sideband decoders.
	 */
	enum pt_sb_abi abi;

	/* We identify processes by their process id.
	 *
	 * Intel PT provides CR3 and VMCS Base to identify address-spaces and
	 * notifies us about changes.  But at least on Linux, we don't get the
	 * CR3 and all sideband records refer to pid/tid, so we're using those.
	 */
	uint32_t pid;

	/* The number of current users.
	 *
	 * We remove a context when the process exits but we keep the context
	 * object and its image alive as long as they are used.
	 */
	uint16_t ucount;
};

/* Allocate a context.
 *
 * Allocate a context and an image.  The optional @name argument is given to the
 * context's image.
 *
 * The context's use-count is initialized to one.  Use pt_sb_ctx_put() to free
 * the returned context and its image.
 *
 * Returns a non-NULL context or NULL when out of memory.
 */
extern struct pt_sb_context *pt_sb_ctx_alloc(const char *name);

#endif /* PT_SB_CONTEXT_H */
