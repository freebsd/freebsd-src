/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Arm Ltd
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

#ifndef _ARM64_ARM_SPE_H_
#define _ARM64_ARM_SPE_H_

/* kqueue events */
#define ARM_SPE_KQ_BUF		138
#define ARM_SPE_KQ_SHUTDOWN	139
#define ARM_SPE_KQ_SIGNAL	140

/* spe_backend_read() u64 data encoding */
#define KQ_BUF_POS_SHIFT	0
#define KQ_BUF_POS		(1 << KQ_BUF_POS_SHIFT)
#define KQ_PARTREC_SHIFT	1
#define KQ_PARTREC		(1 << KQ_PARTREC_SHIFT)
#define KQ_FINAL_BUF_SHIFT	2
#define KQ_FINAL_BUF		(1 << KQ_FINAL_BUF_SHIFT)

enum arm_spe_ctx_field {
	ARM_SPE_CTX_NONE,
	ARM_SPE_CTX_PID,
	ARM_SPE_CTX_CPU_ID
};

enum arm_spe_profiling_level {
	ARM_SPE_KERNEL_AND_USER,
	ARM_SPE_KERNEL_ONLY,
	ARM_SPE_USER_ONLY
};
struct arm_spe_config {
	/* Minimum interval is IMP DEF up to maximum 24 bit value */
	uint32_t interval;

	/* Profile kernel (EL1), userspace (EL0) or both */
	enum arm_spe_profiling_level level;

	/*
	 * Configure context field in SPE records to store either the
	 * current PID, the CPU ID or neither
	 *
	 * In PID mode, kernel threads without a process context are
	 * logged as PID 0
	 */
	enum arm_spe_ctx_field ctx_field;
};

struct arm_spe_svc_buf {
	uint32_t ident;
	uint8_t buf_idx : 1;
};

#endif /* _ARM64_ARM_SPE_H_ */
