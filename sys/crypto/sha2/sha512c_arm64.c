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

#include "sha512.h"
#include "sha512c_impl.h"

void __hidden
SHA512_Transform_arm64_impl(uint64_t * state,
    const unsigned char block[SHA512_BLOCK_LENGTH], const uint64_t K[80])
{
	uint64x2_t W[8];
	uint64x2_t S[4];
	uint64x2_t S_start[4];
	uint64x2_t K_tmp, S_tmp;
	int i;

#define	A64_LOAD_W(x)							\
    W[x] = vld1q_u64((const uint64_t *)(&block[(x) * 16]));		\
    W[x] = vreinterpretq_u64_u8(vrev64q_u8(vreinterpretq_u8_u64(W[x])))

	/* 1. Prepare the first part of the message schedule W. */
	A64_LOAD_W(0);
	A64_LOAD_W(1);
	A64_LOAD_W(2);
	A64_LOAD_W(3);
	A64_LOAD_W(4);
	A64_LOAD_W(5);
	A64_LOAD_W(6);
	A64_LOAD_W(7);

	/* 2. Initialize working variables. */
	S[0] = vld1q_u64(&state[0]);
	S[1] = vld1q_u64(&state[2]);
	S[2] = vld1q_u64(&state[4]);
	S[3] = vld1q_u64(&state[6]);

	S_start[0] = S[0];
	S_start[1] = S[1];
	S_start[2] = S[2];
	S_start[3] = S[3];

	/* 3. Mix. */
	for (i = 0; i < 80; i += 16) {
		/*
		 * The schedule array has 4 vectors:
		 *  ab = S[( 8 - i) % 4]
		 *  cd = S[( 9 - i) % 4]
		 *  ef = S[(10 - i) % 4]
		 *  gh = S[(11 - i) % 4]
		 *
		 * The following maacro:
		 *  - Loads the round constants
		 *  - Add them to schedule words
		 *  - Rotates the total to switch the order of the two halves
		 *    so they are in the correct order for gh
		 *  - Fix the alignment
		 *   - Extract fg from ef and gh
		 *   - Extract de from cd and ef
		 * - Pass these into the first part of the sha512 calculation
		 *   to calculate the Sigma 1 and Ch steps
		 * - Calculate the Sigma 0 and Maj steps and store to gh
		 * - Add the first part to the cd vector
		 */
#define	A64_RNDr(S, W, i, ii)						\
    K_tmp = vld1q_u64(K + (i * 2) + ii);				\
    K_tmp = vaddq_u64(W[i], K_tmp);					\
    K_tmp = vextq_u64(K_tmp, K_tmp, 1);					\
    K_tmp = vaddq_u64(K_tmp, S[(11 - i) % 4]);				\
    S_tmp = vsha512hq_u64(K_tmp,					\
      vextq_u64(S[(10 - i) % 4], S[(11 - i) % 4], 1),			\
      vextq_u64(S[(9 - i) % 4], S[(10 - i) % 4], 1));			\
    S[(11 - i) % 4] = vsha512h2q_u64(S_tmp, S[(9 - i) % 4], S[(8 - i) % 4]); \
    S[(9 - i) % 4] = vaddq_u64(S[(9 - i) % 4], S_tmp)

		A64_RNDr(S, W, 0, i);
		A64_RNDr(S, W, 1, i);
		A64_RNDr(S, W, 2, i);
		A64_RNDr(S, W, 3, i);
		A64_RNDr(S, W, 4, i);
		A64_RNDr(S, W, 5, i);
		A64_RNDr(S, W, 6, i);
		A64_RNDr(S, W, 7, i);

		if (i == 64)
			break;

		/*
		 * Perform the Message schedule computation:
		 * - vsha512su0q_u64 performs the sigma 0 half and add it to
		 *   the old value
		 * - vextq_u64 fixes the alignment of the vectors
		 * - vsha512su1q_u64 performs the sigma 1 half and adds it
		 *   and both the above all together
		 */
#define A64_MSCH(x)							\
    W[x] = vsha512su1q_u64(						\
      vsha512su0q_u64(W[x], W[(x + 1) % 8]),				\
      W[(x + 7) % 8],							\
      vextq_u64(W[(x + 4) % 8], W[(x + 5) % 8], 1))

		A64_MSCH(0);
		A64_MSCH(1);
		A64_MSCH(2);
		A64_MSCH(3);
		A64_MSCH(4);
		A64_MSCH(5);
		A64_MSCH(6);
		A64_MSCH(7);
	}

	/* 4. Mix local working variables into global state */
	S[0] = vaddq_u64(S[0], S_start[0]);
	S[1] = vaddq_u64(S[1], S_start[1]);
	S[2] = vaddq_u64(S[2], S_start[2]);
	S[3] = vaddq_u64(S[3], S_start[3]);

	vst1q_u64(&state[0], S[0]);
	vst1q_u64(&state[2], S[1]);
	vst1q_u64(&state[4], S[2]);
	vst1q_u64(&state[6], S[3]);
}
