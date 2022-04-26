/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Dmitry Chagin <dchagin@FreeBSD.org>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif


enum linux_rseq_cpu_id_state {
	LINUX_RSEQ_CPU_ID_UNINITIALIZED			= -1,
	LINUX_RSEQ_CPU_ID_REGISTRATION_FAILED		= -2,
};

enum linux_rseq_flags {
	LINUX_RSEQ_FLAG_UNREGISTER			= (1 << 0),
};

enum linux_rseq_cs_flags_bit {
	LINUX_RSEQ_CS_FLAG_NO_RESTART_ON_PREEMPT_BIT	= 0,
	LINUX_RSEQ_CS_FLAG_NO_RESTART_ON_SIGNAL_BIT	= 1,
	LINUX_RSEQ_CS_FLAG_NO_RESTART_ON_MIGRATE_BIT	= 2,
};

enum linux_rseq_cs_flags {
	LINUX_RSEQ_CS_FLAG_NO_RESTART_ON_PREEMPT	=
		(1U << LINUX_RSEQ_CS_FLAG_NO_RESTART_ON_PREEMPT_BIT),
	LINUX_RSEQ_CS_FLAG_NO_RESTART_ON_SIGNAL	=
		(1U << LINUX_RSEQ_CS_FLAG_NO_RESTART_ON_SIGNAL_BIT),
	LINUX_RSEQ_CS_FLAG_NO_RESTART_ON_MIGRATE	=
		(1U << LINUX_RSEQ_CS_FLAG_NO_RESTART_ON_MIGRATE_BIT),
};

struct linux_rseq_cs {
	uint32_t version;
	uint32_t flags;
	uint64_t start_ip;
	uint64_t post_commit_offset;
	uint64_t abort_ip;
} __attribute__((aligned(4 * sizeof(uint64_t))));

struct linux_rseq {
	uint32_t cpu_id_start;
	uint32_t cpu_id;
	uint64_t rseq_cs;
	uint32_t flags;
} __attribute__((aligned(4 * sizeof(uint64_t))));

int
linux_rseq(struct thread *td, struct linux_rseq_args *args)
{

	return (ENOSYS);
}
