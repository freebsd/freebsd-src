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

#ifndef PT_SB_DECODER_H
#define PT_SB_DECODER_H

#include <stdio.h>


/* An Intel PT sideband decoder. */
struct pt_sb_decoder {
	/* The next Intel PT sideband decoder in a linear list of Intel PT
	 * sideband decoders ordered by @tsc (ascending).
	 */
	struct pt_sb_decoder *next;

	/* The timestamp of the next sideband record. */
	uint64_t tsc;

	/* Decoder functions provided by the decoder supplier:
	 *
	 * - fetch the next sideband record.
	 */
	int (*fetch)(struct pt_sb_session *session, uint64_t *tsc, void *priv);

	/* - apply the current sideband record. */
	int (*apply)(struct pt_sb_session *session, struct pt_image **image,
		     const struct pt_event *event, void *priv);

	/* - print the current sideband record. */
	int (*print)(struct pt_sb_session *session, FILE *stream,
		     uint32_t flags, void *priv);

	/* - destroy the decoder's private data. */
	void (*dtor)(void *priv);

	/* Decoder-specific private data. */
	void *priv;

	/* A flag saying whether this is a primary or secondary decoder. */
	uint32_t primary:1;
};

#endif /* PT_SB_DECODER_H */
