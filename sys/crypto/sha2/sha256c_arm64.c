/*-
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * This software was developed by Andrew Turner under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/types.h>

#include <arm_neon.h>

#include "sha256c_impl.h"

void __hidden
SHA256_Transform_arm64_impl(uint32_t * state, const unsigned char block[64],
    const uint32_t K[64])
{
	uint32x4_t W[4];
	uint32x4_t S[2];
	uint32x4_t S_start[2];
	uint32x4_t K_tmp, S_tmp;
	int i;

#define	A64_LOAD_W(x)							\
    W[x] = vld1q_u32((const uint32_t *)(&block[(x) * 16]));		\
    W[x] = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(W[x])))

	/* 1. Prepare the first part of the message schedule W. */
	A64_LOAD_W(0);
	A64_LOAD_W(1);
	A64_LOAD_W(2);
	A64_LOAD_W(3);

	/* 2. Initialize working variables. */
	S[0] = vld1q_u32(&state[0]);
	S[1] = vld1q_u32(&state[4]);

	S_start[0] = S[0];
	S_start[1] = S[1];

	/* 3. Mix. */
	for (i = 0; i < 64; i += 16) {
#define	A64_RNDr(i, ii)							\
    K_tmp = vaddq_u32(W[i], vld1q_u32(&K[ii + i * 4]));			\
    S_tmp = vsha256hq_u32(S[0], S[1], K_tmp);				\
    S[1] = vsha256h2q_u32(S[1], S[0], K_tmp);				\
    S[0] = S_tmp

		A64_RNDr(0, i);
		A64_RNDr(1, i);
		A64_RNDr(2, i);
		A64_RNDr(3, i);

		if (i == 48)
			break;

#define	A64_MSCH(x)							\
    W[x] = vsha256su0q_u32(W[x], W[(x + 1) % 4]);			\
    W[x] = vsha256su1q_u32(W[x], W[(x + 2) % 4], W[(x + 3) % 4])

		A64_MSCH(0);
		A64_MSCH(1);
		A64_MSCH(2);
		A64_MSCH(3);
	}

	/* 4. Mix local working variables into global state */
	S[0] = vaddq_u32(S[0], S_start[0]);
	S[1] = vaddq_u32(S[1], S_start[1]);

	vst1q_u32(&state[0], S[0]);
	vst1q_u32(&state[4], S[1]);
}
